'use strict';

let observer;
rpc.exports.init = function (stage, parameters) {
  const payload = Module.load(parameters.nativeDirectory + '/libpurehermes_demo.so');
  const start = new NativeFunction(
    payload.getExportByName('purehermes_demo_start'), 'int', ['pointer']);
  let started = false;
  observer = Process.attachModuleObserver({
    onAdded(module) {
      if (module.name === 'libhermesvm.so' && !started) {
        started = true;
        send({result: start(module.base)});
      }
    }
  });
};
