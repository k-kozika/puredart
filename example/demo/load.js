'use strict';

// Frida only loads the native library; puredart installs the hook itself.
const demo = Module.load('@PUREDART_NATIVE_DIR@/libpuredart_demo.so');
const start = new NativeFunction(demo.getExportByName('puredart_demo_start'), 'int', []);
const status = start();
if (status < 0) throw new Error('puredart initialization failed');
console.log('[puredart] native demo started; see PuredartDemo in logcat');
