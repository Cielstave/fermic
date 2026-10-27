'use strict';

const fs = require('node:fs');
const path = require('node:path');
const directory = process.argv[2];
if (!directory) throw new Error('fixture directory is required');

function write(name, source) {
    fs.writeFileSync(path.join(directory, name), source);
}

write('missing-type.fe', 'func f() -> i32 { let x = 1; return x; }');
write('missing-value.fe', 'func f() -> i32 { let x: i32; return x; }');
write('unsupported.fe', 'func f() -> i32 { match (true) { case true { return 1; } case false { return 0; } } return 0; }');
write('generic.fe', 'func f() -> i32 { return other<i32>(1); }');
write('unterminated.fe', 'func f() -> i32 { return 1;');
write('empty.fe', '');
write('depth-limit.fe', 'func f() -> i32 { return ' + '('.repeat(129) + '1' + ')'.repeat(129) + '; }');
write('chain-limit.fe', 'func f() -> i32 { return 1' + '+1'.repeat(65) + '; }');
write('param-limit.fe', 'func f(' + Array.from({length: 17}, (_, index) => `p${index}: i32`).join(',') + ') -> i32 { return 0; }');
write('func-limit.fe', Array.from({length: 129}, (_, index) => `func f${index}() -> void {}`).join('\n'));
write('node-limit.fe', 'func f() -> void {' + 'var x: i32;'.repeat(16_384) + '}');
write('large.fe', 'func f() -> void {' + 'var x: i32;'.repeat(1000) + '}');
