<script>
    import Join from "./pages/Join.svelte";
    import Settings from "./pages/Settings.svelte";
    import Camera from "./pages/Camera.svelte";
    import System from "./pages/System.svelte";

    const links = [
        {href: '/join', title: 'Join network', component: Join, 'visible': false},
        {href: '/', title: 'Settings', component: Settings, 'visible': true},
        {href: '/camera', title: 'Camera', component: Camera, 'visible': true},
        {href: '/system', title: 'System', component: System, 'visible': true},
    ];

    let path = location.pathname;

    $: current = links.find((link) => link.href === path) ?? links[0];

    const navigate = (event, href) => {
        // Let the browser handle new-tab clicks (ctrl, shift, middle button).
        if (event.button !== 0 || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;

        event.preventDefault();
        if (href === path) return;

        history.pushState({}, '', href);
        path = href;
    };
</script>

<svelte:window on:popstate={() => path = location.pathname}/>

<main>
    <h1>BK7252 Camera</h1>

    <nav>
        {#each links as link}
            {#if link.visible}
                <a href={link.href} class:active={link === current} on:click={(event) => navigate(event, link.href)}>{link.title}</a>
            {/if}
        {/each}
    </nav>

    <svelte:component this={current.component}/>
</main>

<style>
    h1 {
        font-size: 2em;
        line-height: 1.1;
    }

    nav {
        display: flex;
        gap: 1rem;
        margin-bottom: 1rem;
        padding-bottom: 0.5rem;
        border-bottom: 1px solid var(--bg-3);
    }

    a.active {
        color: var(--fg-1);
        font-weight: bold;
        text-decoration: none;
    }

    main {
        padding: 1em;
        color: #111;
    }
</style>
