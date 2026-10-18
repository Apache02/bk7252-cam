import './app.css'
import {mount} from 'svelte';
import Page from "./Page.svelte";

const app = new mount(Page, {
    target: document.getElementById('app'),
})

export default app
