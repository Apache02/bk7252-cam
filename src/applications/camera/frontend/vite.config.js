import {defineConfig} from 'vite'
import {svelte} from '@sveltejs/vite-plugin-svelte'
import {viteSingleFile} from "vite-plugin-singlefile"

const settings = {
    mac: "02:00:00:00:00:01",
    ap_ssid: "camera",
    ap_channel: 6,
    ap_enabled: true,
    sta_ssid: "",
    sta_password: "",
    sta_enabled: false,
    led_enabled: true,
    telnet_enabled: true,
}

const frame = `<svg xmlns="http://www.w3.org/2000/svg" width="640" height="480" viewBox="0 0 640 480">
<rect width="640" height="480" fill="#2b3a4a"/>
<path d="M0 0L640 480M640 0L0 480" stroke="#4d6178" stroke-width="2"/>
<text x="320" y="250" fill="#fff" font-family="sans-serif" font-size="32" text-anchor="middle">mock stream</text>
</svg>`

// Stands in for the device on the dev server.
const mockDevice = () => ({
    name: "mock-device",
    configureServer(server) {
        server.middlewares.use("/stream", (req, res) => {
            res.setHeader("Content-Type", "image/svg+xml")
            res.end(frame)
        })

        server.middlewares.use("/settings.json", (req, res) => {
            res.setHeader("Content-Type", "application/json")
            res.end(JSON.stringify(settings))
        })

        server.middlewares.use("/reboot", (req, res) => {
            res.setHeader("Content-Type", "application/json")
            res.end(JSON.stringify({success: Math.random() > 0.5}))
        })

        server.middlewares.use("/reset", (req, res) => {
            res.setHeader("Content-Type", "application/json")
            res.end(JSON.stringify({success: Math.random() > 0.5}))
        })
    },
})

// https://vitejs.dev/config/
export default defineConfig({
    plugins: [svelte(), viteSingleFile(), mockDevice()],
})
