<script>
    import {preventDefault} from 'svelte/legacy';

    import {slide} from 'svelte/transition';

    let loading = true;
    let error = null;
    let data = null;

    setTimeout(async () => {
        try {
            loading = true;
            const response = await fetch("/settings.json");
            if (!response.ok) {
                throw new Error(`Response status: ${response.status}`);
            }
            data = await response.json();
        } catch (err) {
            console.error(err.message);
            error = err.message;
        }
        loading = false;
    }, 200);

    let saving = false;
    let saveMessage = null;

    const handleSubmit = async () => {
        saving = true;
        saveMessage = null;
        try {
            const response = await fetch("/settings", {
                method: "POST",
                headers: {"Content-Type": "application/json"},
                body: JSON.stringify(data),
            });
            const text = await response.text();
            if (!response.ok) {
                throw new Error(text || `Response status: ${response.status}`);
            }
            data = JSON.parse(text);
            saveMessage = "Saved";
        } catch (err) {
            console.error(err.message);
            saveMessage = `Not saved: ${err.message}`;
        }
        saving = false;
    };
</script>

{#if data == null}
    {#if loading}
        loading...
    {:else if error}
        {error}
    {/if}
{:else}
    <form class="form" onsubmit={preventDefault(handleSubmit)}>
        <div class="group">
            <div class="input">
                <label>
                    MAC<br>
                    <input bind:value={data.mac} disabled={true}/>
                </label>
            </div>
        </div>

        <div class="group">
            <div class="input">
                <label class="checkbox">
                    <input type="checkbox" bind:checked={data.ap_enabled}/>
                    AP Enabled
                </label>
            </div>

            {#if data.ap_enabled}
                <div class="input" transition:slide>
                    <label>
                        AP ssid<br>
                        <input bind:value={data.ap_ssid}/>
                    </label>
                </div>
            {/if}
        </div>

        <div class="group">
            <div class="input">
                <label class="checkbox">
                    <input type="checkbox" bind:checked={data.sta_enabled}/>
                    STA Enabled
                </label>
            </div>

            {#if data.sta_enabled}
                <div class="input" transition:slide>
                    <label>
                        STA ssid<br>
                        <input bind:value={data.sta_ssid}/>
                    </label>
                </div>
                <div class="input" transition:slide>
                    <label>
                        STA password<br>
                        <input bind:value={data.sta_password}/>
                    </label>
                </div>
            {/if}
        </div>

        <div class="group">
            <div class="input">
                <label class="checkbox">
                    <input type="checkbox" bind:checked={data.telnet_enabled}/>
                    Telnet Enabled
                </label>
            </div>
        </div>

        <div class="group">
            <div class="input">
                <label class="checkbox">
                    <input type="checkbox" bind:checked={data.led_enabled}/>
                    Led Enabled
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
