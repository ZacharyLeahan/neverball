// SPDX-License-Identifier: Apache-2.0
// Based on espkvm-xbox/tools/xbox-button.mjs at 61f43235c024d9738d9a37342d334512dc74a82c.
// Local additions: bounded multi-button chords and this repository's usage path.
// Node 22+: bounded button test using the existing KVM keyboard transport.
const keys = {
  up: 0x52, down: 0x51, left: 0x50, right: 0x4f,
  start: 0x28, back: 0x2a, a: 0x1d, b: 0x1b, x: 0x06, y: 0x19,
  black: 0x05, white: 0x11, lt: 0x14, rt: 0x08, ls: 0x15, rs: 0x17,
  'ls-up': 0x1a, 'ls-down': 0x16, 'ls-left': 0x04, 'ls-right': 0x07,
  'rs-up': 0x0c, 'rs-down': 0x0e, 'rs-left': 0x0d, 'rs-right': 0x0f,
};
const [host, button, duration = '200'] = process.argv.slice(2);
const ms = Number(duration);
const chord = (button || '').split('+');
if (!host || chord.length > 6 || new Set(chord).size !== chord.length || chord.some(key => !(key in keys)) || !Number.isInteger(ms) || ms < 100 || ms > 2500) {
  console.error('Usage: node scripts/kvm-button.mjs http://esp.local BUTTON[+BUTTON...] [100..2500 ms]');
  console.error('Buttons:', Object.keys(keys).join(', '));
  process.exit(1);
}
const url = new URL('/ws', host);
if (!['http:', 'https:'].includes(url.protocol)) throw new Error('Use an http(s) device URL');
url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
const ws = new WebSocket(url);
let released = false;
const deadline = setTimeout(() => {
  console.error('Controller connection timed out');
  process.exitCode = 1;
  if (ws.readyState === WebSocket.OPEN) ws.send(new Uint8Array([5]));
  ws.close();
}, 5000);
ws.onopen = () => {
  ws.send(new Uint8Array([7])); // Explicitly claim the user-authorized control session.
  const packet = new Uint8Array(8);
  packet[0] = 3;
  chord.forEach((key, index) => { packet[index + 2] = keys[key]; });
  ws.send(packet);
  setTimeout(() => {
    if (ws.readyState !== WebSocket.OPEN) return;
    ws.send(new Uint8Array([5]));
    released = true;
    setTimeout(() => ws.close(), 100);
  }, ms);
};
ws.onerror = () => { console.error('WebSocket error'); process.exitCode = 1; };
ws.onclose = () => {
  clearTimeout(deadline);
  if (!released) process.exitCode = 1;
  else console.log(`Sent ${button} for ${ms} ms, then release (verify on the Xbox).`);
};
