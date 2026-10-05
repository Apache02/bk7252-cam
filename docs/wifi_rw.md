# WiFi RivieraWaves Arch

RivieraWaves is a semiconductor IP vendor (part of CEVA, Inc.) that licenses
WiFi/Bluetooth/UWB baseband and MAC hardware plus the software stack that
drives it — this chip's WiFi silicon and the kernel described here are their
IP, not Beken's own design. "The archive" throughout this document means
`lib/libip_7221u.a`, the compiled RivieraWaves stack we link against and call
into; we never see its source, only its object code and DWARF debug info.

That stack is split into two halves, referred to below as **LMAC** (Lower
MAC — touches hardware registers directly: channels, DMA, the radio) and
**UMAC** (Upper MAC — 802.11 protocol logic: scanning, association, station
state). Both halves run as kernel tasks inside the same archive; see the table
below for which task belongs to which half. This is this archive's own
classification of its tasks, not a hard rule that everything below the line
touches hardware directly and everything above it doesn't — `MM`, for
instance, does plenty of orchestration alongside its register access.

## Overview

A map of the rest of this document — every box below is a section further
down, not a new concept:

```
                              FreeRTOS
                                 |
                +----------------+-----------------+
                |                                  |
           wifi_core                           wifi_kmsg
      (OS Tasks — serializes                (OS Tasks — drains
       RX/TX/control into                     LMAC->host replies,
       the archive)                           woken by app_sema)
                |                                  ^
                | ke_msg_send()                    | mr_kmsg_fwd()
                | rxl_cntrl_evt()                  |
                v                                  |
   +--------------------------------------------------------+
   |            the archive (lib/libip_7221u.a)             |
   |                                                        |
   |   MM   SCAN   SCANU   ME   SM   APM   BAM   RXU        |
   |    \     |      |      |    |    |     |    /          |
   |     +----+------+--Kernel queues (ke_env)---+          |
   |                    driven by Events                    |
   +--------------------------------------------------------+
                |                                  |
                | phy_* calls                      | g_rwnx_connector
                | (PHY boundary)                   | (Host boundary)
                v                                  v
              radio                         wifi_kmsg / host / lwIP
```

## Kernel tasks

| Group | ID | Task  | Description       |
|-------|----|-------|-------------------|
| LMAC  |    |       |                   |
|       | 0  | MM    | Hardware/channel/vif (virtual interface — one WiFi interface instance) control — by far the biggest task, 97 messages |
|       | 1  | SCAN  | LMAC's own channel-by-channel scan, driven by SCANU |
| UMAC  |    |       |                   |
|       | 2  | SCANU | Scan sessions and join — what `wifi_scan`/`wifi_connect` actually talk to |
|       | 3  | ME    | Per-station config, management-frame TX, rate control |
|       | 4  | SM    | Station state machine — what `wifi_connect` drives for association |
|       | 5  | APM   | AP mode — compiled in, unused by this port (STA only) |
|       | 6  | BAM   | Block Ack agreement tracking; also inspects every management frame for ADDBA/DELBA |
|       | 7  | RXU   | Dispatches incoming management/null-data frames to whichever tasks care |
| -     | 8  | API   | Application level |

The IDs are contiguous by group, and the boundary at 7 is load-bearing —
**as observed in this build**: `TASK_LAST_EMB = TASK_RXU = 7`, and
`ke_msg_send()` compares a message's `dest_id` against that number and nothing
else. Nothing here claims 7 is a constant across RivieraWaves versions, only
that it's what this compiled archive does. At or below it, the message goes
onto the kernel's own queue; above it, straight out to the host connector — the
callback table (`g_rwnx_connector`) the host registers so the archive has
somewhere to deliver a message once it decides not to keep it. See Host
boundary below.

The ID also forms the message ID: `id = (task << 10) + index`. That is why
`SCANU_START_REQ` is `0x0800` and `SM_CONNECT_REQ` is `0x1002`.

## Kernel queues

All three are `co_list` (the archive's own linked-list type) heads inside
`ke_env`, and all three hold `ke_msg`.

* **queue_sent** — delivered but not yet dispatched. `ke_msg_send()` pushes here
  and raises `KE_EVT_KE_MESSAGE`; `ke_task_schedule()` drains it, looking each
  message up in the destination task's handler table.
* **queue_saved** — dispatched, but the handler returned `KE_MSG_SAVED` because
  the task was in the wrong state to act on it. Retried when the task's state
  changes, which is what makes a request sent too early wait rather than fail.
* **queue_timer** — kernel software timers, sorted by expiry. Each entry is a
  message that will be sent when its time comes. The head's expiry is programmed
  into a MAC compare register, and `KE_EVT_KE_TIMER` fires when it elapses.

## ke_env

`ke_env` is the kernel scheduler's whole mutable state — 28 bytes. That's a
claim about the scheduler specifically, not about the archive as a whole:
other archive-global state exists outside it (`sta_info_tab`, `vif_info_tab`,
`rxu_cntrl_env`, and others already seen in this document's disassembly).

| Offset | Size | Field       | Meaning                                       |
|--------|------|-------------|------------------------------------------------|
| 0      | 4    | evt_field   | pending events, one bit each — see Events below |
| 4      | 8    | queue_sent  | `co_list` — first, last                        |
| 12     | 8    | queue_saved | `co_list`                                      |
| 20     | 8    | queue_timer | `co_list`                                      |

## Events

`evt_field` acts like a software interrupt-pending register — one word in RAM instead
of a peripheral, but the same idea: a bit means "this is waiting", the highest
set bit wins, and a handler clears its own bit once done. The one thing real
hardware gives for free that this does not: hardware wakes the CPU by itself.
Nothing here does — waking the host that runs the scheduler has to be arranged
separately (see the two side effects below).

`N` below is the event's enum index — what `KE_EVT_RESET` etc. actually equal.
The bit it occupies in `evt_field` is `31 - N`, so `KE_EVT_RESET` (N=0) is
**bit 31**, the top of the word, not bit 0. Lower N still means higher
priority: `ke_evt_mask_schedule()` finds the highest *set* bit first, clears
it, calls its handler, then re-reads the field — so a handler may raise
further events and still have them serviced in the same pass, and RESET (top
bit) is seen before anything else whenever it's raised.

Several handlers below are named `txl_*` — **TXL** is the archive's own name
for its TX-lower-MAC path, the part of `MM`'s job that pushes queued frames
out to the radio.

`AC` is **access category** — the four TX priority queues from 802.11e/WMM
(WiFi Multimedia), `AC_BK`/`AC_BE`/`AC_VI`/`AC_VO` (background, best-effort,
video, voice, lowest to highest priority). `AC_BCN` is a fifth ring, real in
this archive (`NX_TXQ_CNT = AC_MAX + NX_BEACONING`, and `NX_BEACONING` was 1
when this archive was built — see `lmac_abi.h`), used only for beacons; this
port never sends on it.

**`BCN`** throughout this document is short for "beacon" —
the periodic broadcast frame that advertises a BSS.

**`TBTT`** is Target
Beacon Transmission Time — the scheduled instant a beacon is due; "primary"
and "secondary" TBTT split that instant into two events so the upper layer has
a moment's warning before it must actually have the beacon ready.

| N | Event | Handler |
|-----|-------|---------|
| 0 | `KE_EVT_RESET` | `rwnxl_reset_evt` |
| 1 | `KE_EVT_MM_TIMER` | `mm_timer_schedule` |
| 2 | `KE_EVT_KE_TIMER` | `ke_timer_schedule` |
| 3 | `KE_EVT_TXL_PAYLOAD_BCN` | `txl_payload_handle(AC_BCN)` |
| 4–7 | `KE_EVT_TXL_PAYLOAD_AC3…AC0` | `txl_payload_handle(AC_VO…AC_BK)` |
| 8 | `KE_EVT_KE_MESSAGE` | `ke_task_schedule` |
| 9 | `KE_EVT_HW_IDLE` | `mm_hw_idle_evt` |
| 10, 11 | `KE_EVT_PRIMARY_TBTT`, `…SECONDARY_TBTT` | `mm_tbtt_evt` |
| 12 | `KE_EVT_RXUREADY` | `rxu_cntrl_evt` |
| 13 | `KE_EVT_TXFRAME_CFM` | `txl_frame_evt` |
| 14 | `KE_EVT_TXCFM_BCN` | `txl_cfm_evt(AC_BCN)` |
| 15–18 | `KE_EVT_TXCFM_AC3…AC0` | `txl_cfm_evt(AC_VO…AC_BK)` |
| 19 | `KE_EVT_GP_DMA_DL` | `hal_dma_evt(DMA_DL)` |
| 20 | `KE_EVT_EVM_VIA_MAC_TEST` | `evm_via_mac_evt` |

Three things raise these bits: a MAC **FIQ** handler (most of them — FIQ is
this ARM core's fast interrupt, the one this chip uses for the MAC block),
the host directly (`dma_push()` raises the payload-ready bits with no
interrupt involved), and the archive's own handlers while the scheduler is
already running (a handler may raise the next event itself).

Two side effects hang off `ke_evt_set()` rather than off the schedulers:

* if `evt_field` **was empty**, it calls `bmsg_null_sender()` — the host's hook
  for "wake the task that runs the scheduler". If the field was already
  non-empty it stays silent, on the assumption a scheduler run is already
  coming. See Known hazards below for the one case where that assumption
  isn't safe.
* if the new bit falls outside `core_evt_mask`, it calls `app_set_sema()` to
  wake the second host task instead. `core_evt_mask` is `0xFFFFFFFF` in this
  build, so in practice **this specific call site** never fires. `app_set_sema()`
  is also called from a second, unrelated place — `mr_kmsg_fwd()` — and that
  one fires constantly; it's what actually wakes `wifi_kmsg` (see OS Tasks
  below). Same function, two independent callers; only one of the two is dead
  code here.

## Messages

Full catalog of message ids, from the archive's own task headers, cross-checked
against the compiled `.o` members in `lib/libip_7221u.a` — see the per-task
notes below for what actually got compiled into this build. Deliberately id +
name only here, no payload structs or per-message write-ups — those come later,
one task at a time.

A message's id is `KE_FIRST_MSG(task) + index` — `index` is just its position
in the task's enum, starting at 0 (see Kernel tasks above for `KE_FIRST_MSG`).
This id and `dest_id` (see `struct ke_msg` under Kernel queues) are separate
fields: id fixes which task's enum a name belongs to, but says nothing about
who receives any particular message built with it — that's `dest_id`, chosen
by whoever calls `ke_msg_alloc()`, independently. `RXU_MGT_IND` below is the
clearest case where this matters. Three suffixes cover almost every name:

- **`_REQ`** — arrives at the named task. Sent by whoever wants it done — the
  host, unless the table below marks it internal.
- **`_CFM`** — leaves the named task, back to whoever sent the `_REQ`.
- **`_IND`** (also `_RSP`, `_NTF`) — leaves the named task unprompted, usually
  bound for the host or for another task.

Every task's own header marks a point past which "no API messages should be
defined" — those only ever travel between tasks inside the archive, never to
the host. Marked as **internal** below.

One id, several possible destinations: **`RXU_MGT_IND`** (an incoming
management frame) is built and sent from exactly one place in the whole
archive — `rxu_mgt_frame_check()` in `rxu_cntrl.o`, confirmed by searching
every compiled object for its id. That function classifies the frame first,
then calls `ke_msg_alloc(RXU_MGT_IND, dest_id, ...)` with a `dest_id` it
computes from the classification, and sends once. It is **not** delivered to
several tasks at once — each incoming frame goes to exactly one destination,
picked per frame.

That there's a choice to make at all is why `SCANU`, `SM`, and `BAM` each
define their own `rxu_mgt_ind_handler` — three different possible
destinations for the same id, not three simultaneous recipients of the same
message. Which frame types map to which of the three is not traced past this
point; this document confirms the dispatch is per-frame and single-recipient,
not what the exact routing rule is.

Two headers exist in source but compiled to nothing in this archive:
`tdls_task.o` and `bam_task.o`'s counterpart for TDLS never got real content —
`TDLS_ENABLE` was 0 — and `mesh_task.o` was never even compiled as an archive
member. Neither task has a slot in `ke_task_id` in this build; skip both.

#### MM (task 0)

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x0000` | `MM_RESET_REQ` | |
| 1 | `0x0001` | `MM_RESET_CFM` | |
| 2 | `0x0002` | `MM_START_REQ` | |
| 3 | `0x0003` | `MM_START_CFM` | |
| 4 | `0x0004` | `MM_VERSION_REQ` | |
| 5 | `0x0005` | `MM_VERSION_CFM` | |
| 6 | `0x0006` | `MM_ADD_IF_REQ` | |
| 7 | `0x0007` | `MM_ADD_IF_CFM` | |
| 8 | `0x0008` | `MM_REMOVE_IF_REQ` | |
| 9 | `0x0009` | `MM_REMOVE_IF_CFM` | |
| 10 | `0x000A` | `MM_STA_ADD_REQ` | |
| 11 | `0x000B` | `MM_STA_ADD_CFM` | |
| 12 | `0x000C` | `MM_STA_DEL_REQ` | |
| 13 | `0x000D` | `MM_STA_DEL_CFM` | |
| 14 | `0x000E` | `MM_SET_FILTER_REQ` | |
| 15 | `0x000F` | `MM_SET_FILTER_CFM` | |
| 16 | `0x0010` | `MM_SET_CHANNEL_REQ` | |
| 17 | `0x0011` | `MM_SET_CHANNEL_CFM` | |
| 18 | `0x0012` | `MM_SET_DTIM_REQ` | |
| 19 | `0x0013` | `MM_SET_DTIM_CFM` | |
| 20 | `0x0014` | `MM_SET_BEACON_INT_REQ` | |
| 21 | `0x0015` | `MM_SET_BEACON_INT_CFM` | |
| 22 | `0x0016` | `MM_SET_BASIC_RATES_REQ` | |
| 23 | `0x0017` | `MM_SET_BASIC_RATES_CFM` | |
| 24 | `0x0018` | `MM_SET_BSSID_REQ` | |
| 25 | `0x0019` | `MM_SET_BSSID_CFM` | |
| 26 | `0x001A` | `MM_SET_EDCA_REQ` | |
| 27 | `0x001B` | `MM_SET_EDCA_CFM` | |
| 28 | `0x001C` | `MM_SET_MODE_REQ` | |
| 29 | `0x001D` | `MM_SET_MODE_CFM` | |
| 30 | `0x001E` | `MM_SET_VIF_STATE_REQ` | |
| 31 | `0x001F` | `MM_SET_VIF_STATE_CFM` | |
| 32 | `0x0020` | `MM_SET_SLOTTIME_REQ` | |
| 33 | `0x0021` | `MM_SET_SLOTTIME_CFM` | |
| 34 | `0x0022` | `MM_SET_IDLE_REQ` | |
| 35 | `0x0023` | `MM_SET_IDLE_CFM` | |
| 36 | `0x0024` | `MM_KEY_ADD_REQ` | |
| 37 | `0x0025` | `MM_KEY_ADD_CFM` | |
| 38 | `0x0026` | `MM_KEY_DEL_REQ` | |
| 39 | `0x0027` | `MM_KEY_DEL_CFM` | |
| 40 | `0x0028` | `MM_BA_ADD_REQ` | |
| 41 | `0x0029` | `MM_BA_ADD_CFM` | |
| 42 | `0x002A` | `MM_BA_DEL_REQ` | |
| 43 | `0x002B` | `MM_BA_DEL_CFM` | |
| 44 | `0x002C` | `MM_PRIMARY_TBTT_IND` | |
| 45 | `0x002D` | `MM_SECONDARY_TBTT_IND` | |
| 46 | `0x002E` | `MM_SET_POWER_REQ` | |
| 47 | `0x002F` | `MM_SET_POWER_CFM` | |
| 48 | `0x0030` | `MM_DBG_TRIGGER_REQ` | |
| 49 | `0x0031` | `MM_SET_PS_MODE_REQ` | |
| 50 | `0x0032` | `MM_SET_PS_MODE_CFM` | |
| 51 | `0x0033` | `MM_CHAN_CTXT_ADD_REQ` | |
| 52 | `0x0034` | `MM_CHAN_CTXT_ADD_CFM` | |
| 53 | `0x0035` | `MM_CHAN_CTXT_DEL_REQ` | |
| 54 | `0x0036` | `MM_CHAN_CTXT_DEL_CFM` | |
| 55 | `0x0037` | `MM_CHAN_CTXT_LINK_REQ` | |
| 56 | `0x0038` | `MM_CHAN_CTXT_LINK_CFM` | |
| 57 | `0x0039` | `MM_CHAN_CTXT_UNLINK_REQ` | |
| 58 | `0x003A` | `MM_CHAN_CTXT_UNLINK_CFM` | |
| 59 | `0x003B` | `MM_CHAN_CTXT_UPDATE_REQ` | |
| 60 | `0x003C` | `MM_CHAN_CTXT_UPDATE_CFM` | |
| 61 | `0x003D` | `MM_CHAN_CTXT_SCHED_REQ` | |
| 62 | `0x003E` | `MM_CHAN_CTXT_SCHED_CFM` | |
| 63 | `0x003F` | `MM_BCN_CHANGE_REQ` | |
| 64 | `0x0040` | `MM_BCN_CHANGE_CFM` | |
| 65 | `0x0041` | `MM_TIM_UPDATE_REQ` | |
| 66 | `0x0042` | `MM_TIM_UPDATE_CFM` | |
| 67 | `0x0043` | `MM_CONNECTION_LOSS_IND` | |
| 68 | `0x0044` | `MM_CHANNEL_SWITCH_IND` | |
| 69 | `0x0045` | `MM_CHANNEL_PRE_SWITCH_IND` | |
| 70 | `0x0046` | `MM_REMAIN_ON_CHANNEL_REQ` | |
| 71 | `0x0047` | `MM_REMAIN_ON_CHANNEL_CFM` | |
| 72 | `0x0048` | `MM_REMAIN_ON_CHANNEL_EXP_IND` | |
| 73 | `0x0049` | `MM_PS_CHANGE_IND` | |
| 74 | `0x004A` | `MM_TRAFFIC_REQ_IND` | |
| 75 | `0x004B` | `MM_SET_PS_OPTIONS_REQ` | |
| 76 | `0x004C` | `MM_SET_PS_OPTIONS_CFM` | |
| 77 | `0x004D` | `MM_P2P_VIF_PS_CHANGE_IND` | |
| 78 | `0x004E` | `MM_CSA_COUNTER_IND` | |
| 79 | `0x004F` | `MM_CHANNEL_SURVEY_IND` | |
| 80 | `0x0050` | `MM_BFMER_ENABLE_REQ` | |
| 81 | `0x0051` | `MM_SET_P2P_NOA_REQ` | |
| 82 | `0x0052` | `MM_SET_P2P_OPPPS_REQ` | |
| 83 | `0x0053` | `MM_SET_P2P_NOA_CFM` | |
| 84 | `0x0054` | `MM_SET_P2P_OPPPS_CFM` | |
| 85 | `0x0055` | `MM_P2P_NOA_UPD_IND` | |
| 86 | `0x0056` | `MM_CFG_RSSI_REQ` | |
| 87 | `0x0057` | `MM_RSSI_STATUS_IND` | |
| 88 | `0x0058` | `MM_CSA_FINISH_IND` | |
| 89 | `0x0059` | `MM_CSA_TRAFFIC_IND` | |
| 90 | `0x005A` | `MM_MU_GROUP_UPDATE_REQ` | |
| 91 | `0x005B` | `MM_MU_GROUP_UPDATE_CFM` | |
| 92 | `0x005C` | `MM_FORCE_IDLE_REQ` | internal from here — no more API messages below this point |
| 93 | `0x005D` | `MM_SCAN_CHANNEL_START_IND` | internal — sent by `MM` to `SCAN` (task 1), never seen by the host |
| 94 | `0x005E` | `MM_SCAN_CHANNEL_END_IND` | internal — sent by `MM` to `SCAN` (task 1), never seen by the host |
| 95 | `0x005F` | `MM_GET_CHANNEL_REQ` | internal |
| 96 | `0x0060` | `MM_GET_CHANNEL_CFM` | internal |

#### SCAN (task 1)

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x0400` | `SCAN_START_REQ` | |
| 1 | `0x0401` | `SCAN_START_CFM` | |
| 2 | `0x0402` | `SCAN_DONE_IND` | |
| 3 | `0x0403` | `SCAN_CANCEL_REQ` | |
| 4 | `0x0404` | `SCAN_CANCEL_CFM` | |
| 5 | `0x0405` | `SCAN_TIMER` | internal — kernel timer message, not host-visible |

This is the LMAC's own scan (channel-by-channel probing), distinct from
`SCANU` below (the UMAC scan session `wifi_scan` actually drives).

#### SCANU (task 2)

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x0800` | `SCANU_START_REQ` | |
| 1 | `0x0801` | `SCANU_START_CFM` | |
| 2 | `0x0802` | `SCANU_JOIN_REQ` | |
| 3 | `0x0803` | `SCANU_JOIN_CFM` | |
| 4 | `0x0804` | `SCANU_RESULT_IND` | |
| 5 | `0x0805` | `SCANU_FAST_REQ` | |
| 6 | `0x0806` | `SCANU_FAST_CFM` | |

Also defines its own `rxu_mgt_ind_handler` — one of three possible
destinations for `RXU_MGT_IND`, see the note above.

#### ME (task 3)

`RC_ENABLE` is on in this build (`me_rc_stats_req_handler` and
`me_rc_set_rate_req_handler` are both present in `me_task.o`), so the three
`RC_*` messages are real, not compiled out.

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x0C00` | `ME_CONFIG_REQ` | |
| 1 | `0x0C01` | `ME_CONFIG_CFM` | |
| 2 | `0x0C02` | `ME_CHAN_CONFIG_REQ` | |
| 3 | `0x0C03` | `ME_CHAN_CONFIG_CFM` | |
| 4 | `0x0C04` | `ME_SET_CONTROL_PORT_REQ` | |
| 5 | `0x0C05` | `ME_SET_CONTROL_PORT_CFM` | |
| 6 | `0x0C06` | `ME_TKIP_MIC_FAILURE_IND` | |
| 7 | `0x0C07` | `ME_MGMT_TX_REQ` | |
| 8 | `0x0C08` | `ME_MGMT_TX_CFM` | |
| 9 | `0x0C09` | `ME_MGMT_TX_DONE_IND` | |
| 10 | `0x0C0A` | `ME_STA_ADD_REQ` | |
| 11 | `0x0C0B` | `ME_STA_ADD_CFM` | |
| 12 | `0x0C0C` | `ME_STA_DEL_REQ` | |
| 13 | `0x0C0D` | `ME_STA_DEL_CFM` | |
| 14 | `0x0C0E` | `ME_TX_CREDITS_UPDATE_IND` | |
| 15 | `0x0C0F` | `ME_UAPSD_TRAFFIC_IND_REQ` | |
| 16 | `0x0C10` | `ME_UAPSD_TRAFFIC_IND_CFM` | |
| 17 | `0x0C11` | `ME_RC_STATS_REQ` | conditional on `RC_ENABLE` — present here |
| 18 | `0x0C12` | `ME_RC_STATS_CFM` | conditional on `RC_ENABLE` — present here |
| 19 | `0x0C13` | `ME_RC_SET_RATE_REQ` | conditional on `RC_ENABLE` — present here |
| 20 | `0x0C14` | `ME_SET_ACTIVE_REQ` | internal from here — no more API messages below this point |
| 21 | `0x0C15` | `ME_SET_ACTIVE_CFM` | internal |
| 22 | `0x0C16` | `ME_SET_PS_DISABLE_REQ` | internal |
| 23 | `0x0C17` | `ME_SET_PS_DISABLE_CFM` | internal |
| 24 | `0x0C18` | `ME_PS_REQ` | internal |

No `ME_MAX` sentinel exists in source — 25 messages is the full count.

#### SM (task 4)

Matches `lmac_abi.h` exactly, id for id, including the vendor's own typo
(`ASSOC_FAIL_INID` at 18, not fixed at 21 either — `ASSOC_FAILED_IND` is a
separate, later message).

SM is a state machine (`enum sm_state_tag` in the archive's own header):
`SM_IDLE → SM_SCANNING → SM_JOINING → SM_STA_ADDING → SM_DISABLING_PS →
SM_BSS_PARAM_SETTING → SM_AUTHENTICATING → SM_ASSOCIATING → SM_ACTIVATING`,
with `SM_DISCONNECTING` as the separate path back down. Most of the messages
below are steps or reports along that chain, not independent operations.

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x1000` | `SM_RESET_REQ` | back to `SM_IDLE` |
| 1 | `0x1001` | `SM_RESET_CFM` | |
| 2 | `0x1002` | `SM_CONNECT_REQ` | starts the chain above — SSID/BSSID/channel/security in, `SM_SCANNING` onward |
| 3 | `0x1003` | `SM_CONNECT_CFM` | only says the request was accepted for processing, not how it turned out |
| 4 | `0x1004` | `SM_CONNECT_IND` | the actual outcome, sent once the chain reaches `SM_ACTIVATING` or fails partway through it |
| 5 | `0x1005` | `SM_DISCONNECT_REQ` | starts `SM_DISCONNECTING` |
| 6 | `0x1006` | `SM_DISCONNECT_CFM` | |
| 7 | `0x1007` | `SM_DISCONNECT_IND` | left the BSS, whether by request or because the link failed |
| 8 | `0x1008` | `SM_POWER_MGMT_REQ` | change the 802.11 power-save mode advertised to the AP |
| 9 | `0x1009` | `SM_POWER_MGMT_CFM` | |
| 10 | `0x100A` | `SM_SYNCLOST_IND` | lost TSF sync with the AP |
| 11 | `0x100B` | `SM_RSP_TIMEOUT_IND` | the AP didn't answer an 802.11 procedure (auth/assoc) in time |
| 12 | `0x100C` | `SM_ROAMING_TIMER_IND` | periodic roaming-check timer, vendor comment says a 10 s period |
| 13 | `0x100D` | `SM_GET_BSS_INFO_REQ` | ask SM for details of the BSS it's currently on |
| 14 | `0x100E` | `SM_GET_BSS_INFO_CFM` | |
| 15 | `0x100F` | `SM_CONNECTION_START_IND` | marks the start of an attempt, ahead of the rest of the chain |
| 16 | `0x1010` | `SM_BEACON_LOSE_IND` | the AP's beacon hasn't been heard for the loss threshold |
| 17 | `0x1011` | `SM_AUTHEN_FAIL_IND` | the 802.11 authentication step failed |
| 18 | `0x1012` | `SM_ASSOC_FAIL_INID` | the 802.11 association step failed — vendor typo in the name, kept verbatim, not `SM_ASSOC_FAILED_IND` (that's 21) |
| 19 | `0x1013` | `SM_ASSOC_IND` | **Observed:** id and name only. **Inferred:** vendor's own header comment on this and the next two rows is the same copy-pasted sentence ("Indicates that the SM associated the AP") for all three — a mistake in the archive's own comments, not something this document introduced. **Unknown:** how this actually differs from `SM_CONNECT_IND`/`SM_DISCONNECT_IND`. |
| 20 | `0x1014` | `SM_DEASSOC_IND` | **Observed:** id and name only, same copy-pasted vendor comment as row 19. **Inferred:** likely means disassociated, from the name. **Unknown:** exact trigger and how it differs from `SM_DISCONNECT_IND`. |
| 21 | `0x1015` | `SM_ASSOC_FAILED_IND` | **Observed:** id and name only, same copy-pasted vendor comment as row 19. **Inferred:** likely distinct from `SM_ASSOC_FAIL_INID` (18) and `SM_AUTHEN_FAIL_IND` (17), from the name. **Unknown:** the actual distinction. |

Also defines its own `rxu_mgt_ind_handler` — one of three possible
destinations for `RXU_MGT_IND`, see the note above.

#### APM (task 5) — AP mode

Compiled in because `NX_BEACONING` was on when this archive was built — a
build-time choice about the archive itself, unrelated to what any particular
application does with it. (Separately: this port's own STA-only
`wifi_connect_start` never calls into APM — an application-level fact, not a
consequence of the archive's build.) `apm_start_req_handler`,
`_start_cac_req_handler`, `_stop_req_handler`, `_stop_cac_req_handler` are the
only handlers actually present in `apm_task.o` — the three `_IND` rows are
sent by APM, never received by it, so they need no local handler.

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x1400` | `APM_START_REQ` | |
| 1 | `0x1401` | `APM_START_CFM` | |
| 2 | `0x1402` | `APM_STOP_REQ` | |
| 3 | `0x1403` | `APM_STOP_CFM` | |
| 4 | `0x1404` | `APM_START_CAC_REQ` | |
| 5 | `0x1405` | `APM_START_CAC_CFM` | |
| 6 | `0x1406` | `APM_STOP_CAC_REQ` | |
| 7 | `0x1407` | `APM_STOP_CAC_CFM` | |
| 8 | `0x1408` | `APM_ASSOC_IND` | sent by APM, no local handler needed |
| 9 | `0x1409` | `APM_DEASSOC_IND` | sent by APM, no local handler needed |
| 10 | `0x140A` | `APM_ASSOC_FAILED_IND` | sent by APM, no local handler needed |

#### BAM (task 6) — Block Ack manager

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x1800` | `BAM_ADD_BA_RSP_TIMEOUT_IND` | |
| 1 | `0x1801` | `BAM_INACTIVITY_TIMEOUT_IND` | |

Only two ids of its own, but BAM also defines its own `rxu_mgt_ind_handler` —
one of three possible destinations for `RXU_MGT_IND`, see the note above.
That handler, not either message above, is how BAM actually finds out about
ADDBA/DELBA action frames.

Whatever Block Ack accounting BAM keeps on outgoing frames draws on the
archive's own internal, per-AC TX-confirm events — `KE_EVT_TXFRAME_CFM`,
`KE_EVT_TXCFM_AC3…AC0`, `KE_EVT_TXCFM_BCN` (see Events above) — not on
`tx_confirm_func` in Host boundary. That one is a separate, host-facing
signal: coarser, no argument, purely for the host's own buffer bookkeeping.

#### RXU (task 7)

| Idx | Value | Message | Notes |
|----|-------|---------|-------|
| 0 | `0x1C00` | `RXU_MGT_IND` | sent to exactly one of `SCANU`/`SM`/`BAM` per frame, chosen by `rxu_mgt_frame_check()` — see above |
| 1 | `0x1C01` | `RXU_NULL_DATA` | |

## Host boundary

This is LMAC→host only, in this build. Host→LMAC never needs any of this —
it's just `ke_msg_send()` with `dest_id ≤ 7`, straight onto the kernel's own
`queue_sent` (see Kernel tasks above).

As observed here, the entire boundary is one comparison in `ke_msg_send()`:
read the message's `dest_id`, and check it against 7 — the same 7 as
`TASK_LAST_EMB` in Kernel tasks above. `dest_id > 7`: hand the message to
`g_rwnx_connector.msg_outbound_func` (dropped if that pointer isn't set).
`dest_id <= 7`: push it onto the kernel's own `queue_sent` and raise
`KE_EVT_KE_MESSAGE` instead — it never leaves the archive.

`g_rwnx_connector` is a struct of five function pointers, defined by the
archive and filled in by the host. Ours, in `rwnx_intf.c`:

| Field | Our implementation | What it does |
|---|---|---|
| `msg_outbound_func` | `mr_kmsg_fwd` | control message → the `wifi_kmsg` task's queue |
| `data_outbound_func` | `data_outbound_stub` | a received frame → `wifi_net_rx` → lwIP |
| `rx_alloc_func` | `rx_alloc_stub` | the archive wants an RX buffer — hand it a real `pbuf` |
| `get_rx_valid_status_func` | always 1 | "room to receive?" |
| `tx_confirm_func` | a counter | a TX finished — carries no argument, so buffers are reclaimed another way (their descriptor coming back around), this only counts |

Registering is not once-and-done. `rwnx_connector_init()` has to run before
the WiFi core tasks start, **and again after every `MM_RESET_REQ`** — the
archive's own reset path zeroes `g_rwnx_connector` internally as a side
effect. Miss the re-registration and every LMAC→host message silently
vanishes from that point on, forever, with no error. `wifi_core.c`'s message
loop already does this — it re-registers right after the scheduler runs a
message it recognizes as `MM_RESET_REQ`.

## PHY boundary

A second seam, symmetric to Host boundary above but facing the radio instead
of the host application. The archive defines none of the nine `phy_*`
functions it calls — `phy_init`, `phy_set_channel`, `phy_stop`,
`phy_get_channel`, `phy_get_nss`, `phy_get_ntx`, `phy_get_rf_gain_capab`,
`phy_get_rf_gain_idx`, `phy_get_version` — all nine are undefined symbols the
archive expects the platform to supply (confirmed against the compiled
archive, not just its headers).

`MM` is almost the only caller: `phy_init` and `phy_stop` bracket its own
startup/teardown, `phy_set_channel` runs whenever `MM` (or `chan.o`, or
`mm_bcn.o` while building a beacon) moves to a different operating channel.
`phy_get_channel` alone is read much more widely — `ps.o`, `rxl_cntrl.o`,
`txl_frame.o`, `bam.o`, `rxu_cntrl.o` — anywhere code needs to know which
channel is currently active.

What's on the other side of this seam — the actual TRX/modem register work —
is not the archive's concern at all, and so isn't this document's either; it
belongs in a hardware-level reference, not here.

## OS Tasks

* **wifi_core** — the FreeRTOS execution context that serializes access to
  the non-thread-safe archive kernel. Three message flows funnel through it:

  | Code | Flow | Source | `wifi_core` calls | Destination |
  |---|---|---|---|---|
  | `BMSG_NULL_TYPE` (0) | none | — | nothing — only the `ke_evt_core_scheduler()` call below | — |
  | `BMSG_RX_TYPE` (1) | RX | the archive itself (`bmsg_rx_sender`) | `rxl_cntrl_evt()` | kernel |
  | `BMSG_TX_TYPE` (2) | TX | lwIP (`ethernetif.c`) | `wifi_net_tx_from_core()` | kernel |
  | `BMSG_IOCTL_TYPE` (3) | control | host (`rw_msg.c`) | `ke_msg_send()` | kernel |

  RX is kernel-to-kernel — the archive signals "frame ready" and `wifi_core`
  hands it straight back in; it's pure middleware there, not a source or a
  destination. The signal carries no data: both of the archive's call sites
  for `bmsg_rx_sender()` pass a literal `0` (confirmed by disassembling
  `rxl_cntrl_evt` and `rxl_timer_int_handler` in `rxl_cntrl.o`), and
  `rxl_cntrl_evt()` on our side never reads its own argument either — hence
  its `int dummy` parameter name. After each message, it calls
  `ke_evt_core_scheduler()` — even
  for `BMSG_NULL_TYPE`, which carries nothing but still triggers that call.
  It blocks on the queue with no timeout, so nothing wakes it on a clock — see
  Known hazards below.
* **wifi_kmsg** — waits on `app_sema`, given by `app_set_sema()` inside
  `mr_kmsg_fwd` (the `msg_outbound_func` from Host boundary above) each time
  the archive hands the host a message. Wakes to call `rwnx_recv_msg()`,
  draining LMAC→host replies, then `ke_evt_none_core_scheduler()` for whatever
  `wifi_core` doesn't cover.

## Known hazards

### Missed wake-up on assert-recovery

`ke_evt_set()` only wakes `wifi_core` on the transition from an empty
`evt_field` to a non-empty one (see Events above). If a bit is already set
when a new one is raised, no wake-up call happens — the assumption is that
`wifi_core` is already on its way to check `evt_field` and will see both bits
when it gets there.

That assumption depends on `wifi_core` actually being woken again by
*something*, eventually. For almost every event, it is: the same kind of
event tends to recur (another RX frame, another timer), and each recurrence
is a fresh chance to be noticed. One event breaks that pattern: the archive's
own assert-recovery path (`hal_assert_rec()`, confirmed by disassembling
`hal_machw.o`) disables the MAC's hardware interrupts *before* raising
`KE_EVT_RESET`. If that particular wake-up is the one that gets lost — a
plain race between "check the queue" and "go back to sleep", not specific to
this event — nothing will ever raise another one to retry it. `wifi_core`
blocks forever, the MAC interrupts stay off, and the archive is dead with the
radio still powered, with no error logged anywhere.

This is a **currently unguarded** hazard, not a hypothetical one:
`core_task_main()` (`wifi_core.c`) blocks on its queue with `portMAX_DELAY` —
no periodic re-check exists in this codebase to catch a lost wake-up. An
earlier version of this port had a 50 ms polling fallback for exactly this
case; it was removed, and nothing replaced it (see OS Tasks above).
