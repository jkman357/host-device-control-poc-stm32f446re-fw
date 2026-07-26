# Global Object Register

## GOR-CORE-001

| Field | Value |
|---|---|
| Global Object ID | `GOR-CORE-001` |
| Symbol | `g_vector_table` |
| Type | `const startup_isr_callback_t[STARTUP_VECTOR_COUNT]` |
| Owner | Startup module |
| Writer | Link-time initialization only |
| Reader | Cortex-M4 exception and interrupt dispatch hardware |
| Synchronization | Immutable after link; no runtime synchronization required |
| Initialization | Linker places the initialized object in `.isr_vector` at the Flash vector origin |
| Lifetime | Entire firmware image lifetime |
| Reason | Required by Cortex-M4 startup and interrupt-vector integration |
| Verification | Linker-layout test, independent target build, and target reset/interrupt testing |

All mutable Product-owned module objects are file-static. No heap allocation is used.
