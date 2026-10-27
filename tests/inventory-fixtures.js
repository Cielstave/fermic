'use strict';
const fs = require('fs');
const path = require('path');
const directory = process.argv[2];
function write(name, contents) {
    fs.writeFileSync(path.join(directory, name), contents);
}
const initial = 'ITEM 1 10 20 Part\n';
const rejected = {
    duplicate: initial + 'ITEM 1 0 0 Other\n',
    missing: 'OUT 9 1\n',
    underflow: initial + 'OUT 1 11\n',
    overflow: 'ITEM 1 1000000 1 Part\nIN 1 1\n',
    nonempty: initial + 'DROP 1\n',
    zero_id: 'ITEM 0 0 0 Part\n',
    number: 'ITEM 1 1000001 0 Part\n',
    spacing: 'IN  1 0\n',
    trailing: initial + 'IN 1 0 \n',
    name: 'ITEM 1 0 0 ' + 'x'.repeat(49) + '\n',
    truncated: 'ITEM 1 0 0 Part',
    line: 'x'.repeat(129) + '\n',
    input: '\n'.repeat(1048577),
    commands: initial + 'IN 1 0\n'.repeat(4096),
    capacity: Array.from({length: 65}, (_, index) => `ITEM ${index + 1} 0 0 Part\n`).join(''),
    utf8: Buffer.concat([Buffer.from('ITEM 1 0 0 '), Buffer.from([255, 10])]),
    nul: Buffer.from('ITEM 1 0 0 A\0B\n'),
};
for (const [name, contents] of Object.entries(rejected)) write(`reject-${name}.journal`, contents);
write('boundary.journal', initial + 'IN 1 0\n'.repeat(4095));
write('boundary.expected', initial);
write('empty.journal', '');
for (let offset = 0; offset < 64; offset++) {
    const item = 'ITEM 1 0 0 ' + 'é'.repeat(24) + '\n';
    write(`chunk-${offset}.journal`, '\n'.repeat(offset) + item);
    write('chunk.expected', item);
}
for (let seed = 1; seed <= 3; seed++) {
    let random = seed;
    function next(limit) {
        random = (Math.imul(random, 1664525) + 1013904223) >>> 0;
        return random % limit;
    }
    const items = new Map();
    const commands = [];
    const names = ['Café beans', 'Steel bolts', 'Valve 😀', 'é label'];
    let identifier = 1;
    while (commands.length < 2048) {
        const action = next(5);
        if ((action === 0 || items.size === 0) && items.size < 64) {
            const item = {id: identifier++, quantity: next(1000), price: next(1000001), name: names[next(names.length)]};
            items.set(item.id, item);
            commands.push(`ITEM ${item.id} ${item.quantity} ${item.price} ${item.name}`);
            continue;
        }
        const item = Array.from(items.values())[next(items.size)];
        if (action === 1) {
            const amount = Math.min(next(1000), 1000000 - item.quantity);
            commands.push(`IN ${item.id} ${amount}`);
            item.quantity += amount;
        } else if (action === 2) {
            const amount = next(item.quantity + 1);
            commands.push(`OUT ${item.id} ${amount}`);
            item.quantity -= amount;
        } else if (action === 3) {
            item.price = next(1000001);
            commands.push(`PRICE ${item.id} ${item.price}`);
        } else if (item.quantity === 0) {
            commands.push(`DROP ${item.id}`);
            items.delete(item.id);
        } else {
            commands.push(`OUT ${item.id} ${item.quantity}`);
            item.quantity = 0;
        }
    }
    const snapshot = Array.from(items.values()).sort((left, right) => left.id - right.id)
        .map(item => `ITEM ${item.id} ${item.quantity} ${item.price} ${item.name}\n`).join('');
    write(`reference-${seed}.journal`, commands.join('\n') + '\n');
    write(`reference-${seed}.expected`, snapshot);
}
