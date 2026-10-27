# Type aliases

A top-level type alias provides another name for an existing type:

```fermi
type Count = usize;
type Header = u8[16];
type Bytes = Buffer<u8>;
type Input = read u8[];

func length(bytes: Input) -> Count { return bytes.len; }
```

Aliases are transparent: `Count` and `usize` have identical type identity and
representation. They are interchangeable without casts. An alias does not create
a distinct ID type or change mutability, ownership, effects, arithmetic checks,
cleanup, or ABI rules. Resource aliases still require moves and cannot be copied.
View aliases retain the restrictions on local storage and escaping returns.

The target must already be available at the declaration. Alias chains are resolved
to the original type immediately. Forward and recursive aliases are not supported.
Aliases cannot be generic declarations in this initial implementation. Up to 1024
aliases are admitted across the project, under the existing source and arena limits.

## Modules and names

Aliases are private unless declared with `pub type`. Imported aliases use the
ordinary module qualifier, such as `types.Count`. A public alias may deliberately
expose a private nominal target under an API name:

```fermi
struct Summary { completed: usize; }
pub type Report = Summary;
```

The public alias admits construction and matching through that name; the original
private name remains inaccessible to other modules. Two aliases of one nominal
type still refer to that same nominal type. Enum cases and struct constructors
accept alias names without introducing a second representation.

An alias cannot reuse a prelude name or conflict with a local type, function,
generic function, or import alias. Duplicate, unknown, malformed, and forward
alias declarations report `E_PARSE`. Import-name conflicts report `E_MODULE`.
Diagnostics and generated C use the resolved target type. Aliases do not generate
additional C typedefs or a new ABI type.

`type` is now a reserved keyword. Existing identifiers using that spelling must
be renamed. See the [runnable example](../../examples/features/type_aliases/main.fe)
and `tests/type-aliases.sh` for scalar, array, nominal, resource, and module cases.
