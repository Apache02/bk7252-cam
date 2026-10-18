<script>
    let rebootMessage = null;
    let resetMessage = null;

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
    .group {
        padding: .5em;
        margin: 1em 0;
        background: #eee;
    }
</style>
