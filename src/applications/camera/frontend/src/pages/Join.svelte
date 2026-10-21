<script>
    import {slide} from 'svelte/transition';

    let networks = [];
    let loading = true;
    let ready = false;
    let error = null;

    const scan = async () => {
        try {
            loading = true;
            const response = await fetch("/scan.json");
            if (!response.ok) {
                throw new Error(`Response status: ${response.status}`);
            }
            networks = (await response.json()).networks || [];
            ready = true;
        } catch (err) {
            console.error(err.message);
            error = err.message;
        }
        loading = false;
    };
    const rescan = async (event) => {
        networks = [];
        event.preventDefault();
        scan();
    }
    setTimeout(scan, 200);

    let sta_ssid = null;
    let sta_password = null;
    let saving = false;
    let saveMessage = null;

    const handleSubmit = async (event) => {
        event.preventDefault();
        saving = true;
        saveMessage = null;
        try {
            const response = await fetch("/setup", {
                method: "POST",
                headers: {"Content-Type": "application/json"},
                body: JSON.stringify({sta_ssid, sta_password: sta_password ?? ""}),
            });
            const text = await response.text();
            if (!response.ok) {
                saveMessage = text || `Response status: ${response.status}`;
                return;
            }
            const {success, network} = JSON.parse(text);
            saveMessage = success
                ? "Saved, new IP: " + network.ip
                : "Fail";
        } catch (err) {
            console.error(err.message);
            saveMessage = `Not saved: ${err.message}`;
        }
        saving = false;
    };
</script>

{#if !ready}
    {#if loading}
        loading...
    {:else if error}
        {error}
    {/if}
{:else}
    <form class="form" onsubmit={handleSubmit}>
        <div class="group">
            <div class="input" transition:slide>
                <label>
                    Select SSID<br>
                    <select bind:value={sta_ssid}>
                        {#each networks as {ssid, security, channel}}
                            <option value="{ssid}">{ssid} ({security}, ch:{channel})</option>
                        {/each}
                    </select>
                    <button onclick={rescan}>refresh</button>
                </label>
            </div>
            <div class="input" transition:slide>
                <label>
                    STA password<br>
                    <input bind:value={sta_password}/>
                </label>
            </div>
        </div>

        <div class="controls">
            <button type="submit" disabled={saving}>Save</button>
            {#if saveMessage}
                <span>{saveMessage}</span>
            {/if}
        </div>
    </form>
{/if}

<style>
    label {
        display: block;
        font-size: 1em;
    }

    .group {
        padding: .5em;
        margin: 1em 0;
        background: #eee;
    }

    .input {
        margin: .5em 0;
    }
</style>
