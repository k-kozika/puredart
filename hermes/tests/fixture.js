var count = 0;
function update(value) { return value + 1; }
function untouched(value) { return value + 1; }
function branches(value) { if(value < 0) return -1; return value + 2; }
function recurse(n) { if(n <= 0) return 0; return recurse(n-1)+1; }
function throws() { throw new Error('expected'); }
var title = '+1';
function click() { count = update(count); return count; }
var object = {title: '+1', number: 7};
var array = ['+1', undefined, 4];
var unicode = '🐘日本';
function text() { return 'unchanged'; }
