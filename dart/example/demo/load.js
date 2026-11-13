'use strict';

// Frida supplies parameters as data; puredart installs the hook itself.
rpc.exports.init = function (stage, parameters) {
  const demo = Module.load(parameters.nativeDirectory + '/libpuredart_demo.so');
  const start = new NativeFunction(demo.getExportByName('puredart_demo_start'), 'int', []);
  const status = start();
  if (status < 0) throw new Error('puredart initialization failed');
  console.log('[puredart] native demo started; see PuredartDemo in logcat');
};
