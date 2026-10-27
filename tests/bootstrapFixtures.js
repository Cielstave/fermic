'use strict';

const fs = require('node:fs');
const path = require('node:path');
const directory = process.argv[2];
if (!directory) throw new Error('fixture directory is required');

function write(name, content) {
    fs.writeFileSync(path.join(directory, name), content);
}

write('empty.fe', '');
write('valid.fe', 'func main() -> i32 { return 0; }');
write('invalid.fe', 'let name = 42;\n?');
write('nul.fe', Buffer.from([47, 42, 0, 42, 47]));
write('source-limit.fe', ' '.repeat(1_048_576));
write('source-overflow.fe', ' '.repeat(1_048_577));
write('token-limit.fe', ';'.repeat(131_071));
write('token-overflow.fe', ';'.repeat(131_072));
// Put an identifier across each position near the 4 KiB read boundary.
for (let offset = 4080; offset < 4112; offset++) {
    write(`chunk-${offset}.fe`, ' '.repeat(offset) + 'identifier_123 /* across boundary */ 42');
}
