<script>
    import {onMount} from "svelte";

    let version = null;
    let network = null;
    let rebootMessage = null;
    let resetMessage = null;

    onMount(async () => {
        try {
            const response = await fetch("/system.json");
            ({version, network} = await response.json());
        } catch (err) {
            console.error(err.message);
        }
    });

    const handleReboot = async () => {
        if (!confirm("Reboot?")) return;

        rebootMessage = null;
        try {
            const response = await fetch("/reboot", {
                method: "POST",
                headers: {"Content-Type": "application/json"},
                body: JSON.stringify({}),
            });
            const {success} = await response.json();
            if (success) {
                rebootMessage = "Rebooting...";
            } else {
                rebootMessage = "Something went wrong...";
            }
        } catch (err) {
            console.error(err.message);
            rebootMessage = `Error: ${err.message}`;
        }
    };
    const handleReset = async () => {
        if (!confirm("Reset?")) return;

        resetMessage = null;
        try {
            const response = await fetch("/reset", {
                method: "POST",
                headers: {"Content-Type": "application/json"},
                body: JSON.stringify({}),
            });
            const {success} = await response.json();
            if (success) {
                resetMessage = "Done";
                setTimeout(() => resetMessage = null, 5000)
            } else {
                resetMessage = "Something went wrong...";
            }
        } catch (err) {
            console.error(err.message);
            resetMessage = `Error: ${err.message}`;
        }
    };
</script>

{#if version}
    <div class="group">
        <div>Version: {version}</div>
    </div>
{/if}

<div class="group">
    <h4>Network</h4>
    <div class="mono">IP: {(network && network.ip) ?? "not connected"}</div>
    {#if network}
        <div class="mono">Mask: {network.mask ?? "undefined"}</div>
        <div class="mono">Gateway: {network.gw ?? "undefined"}</div>
    {/if}
</div>

<div class="group">
    <button on:click={handleReboot}>Reboot</button>
    {#if rebootMessage}
        <span>{rebootMessage}</span>
    {/if}
</div>

<div class="group">
    <button on:click={handleReset}>Reset</button>
    {#if resetMessage}
        <span>{resetMessage}</span>
    {/if}
</div>

<style>
    h4 {
        margin: 0 0 .5em 0;
        font-size: 1em;
        font-weight: normal;
    }
    .group {
        padding: .5em;
        margin: 1em 0;
        background: #eee;
    }
    .mono {
        font-family: monospace;
    }
</style>
