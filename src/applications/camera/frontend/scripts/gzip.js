import {readFileSync, writeFileSync} from 'node:fs'
import {gzipSync} from 'node:zlib'

const input = 'dist/index.html'
const output = `${input}.gz`

writeFileSync(output, gzipSync(readFileSync(input), {level: 9}))
