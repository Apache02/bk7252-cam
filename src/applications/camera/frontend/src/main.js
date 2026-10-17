import './app.css'
import {mount} from 'svelte';
import Settings from "./Settings.svelte";

const app = new mount(Settings, {
    target: document.getElementById('app'),
})

export default app
