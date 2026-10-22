# Linked model and IR libraries

`LibraryManager` persists only absolute file links, a display name, tags, and a caller-defined JSON metadata object. It never copies, deletes, decodes, or loads media. `isMissing()` makes moved/deleted targets explicit; `relink()` validates and replaces a link while retaining its identifier and metadata.

`NamLibrary` accepts `.nam`; `IRLibrary` accepts `.wav`. Persist `toValueTree()` in the application's settings `ValueTree`. Serialization is deliberately UI-independent and can be unit-tested without audio hardware.
