<script>
    import Settings from "./pages/Settings.svelte";
    import Camera from "./pages/Camera.svelte";
    import System from "./pages/System.svelte";

    const links = [
        {href: '/', title: 'Settings', component: Settings},
        {href: '/camera', title: 'Camera', component: Camera},
        {href: '/system', title: 'System', component: System},
    ];

    const current = links.find((link) => link.href === location.pathname) ?? links[0];
</script>

<main>
    <h1>BK7252 Camera</h1>

    <nav>
        {#each links as link}
            <a href={link.href} class:active={link === current}>{link.title}</a>
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
