# RZI Storage API

## 1. Purpose

`include/rzi/storage.h` is the only persistent key-value interface used by RZI
services. Service and AT code must not call Zephyr settings, NVS, or flash APIs
directly. This keeps persistence policy independent of the selected physical
backend.

The current Zephyr adapter is implemented in
`src/storage/storage_settings.c`. `CONFIG_RZI_STORAGE` enables the service;
applications still select and configure the Zephyr settings backend.

## 2. Contract

- `rzi_storage_init()` is idempotent and thread-context only.
- Keys are composed as `<namespace>/<key>`.
- read requires an exact stored-size match and returns `-EMSGSIZE` otherwise.
- write replaces one complete value through the backend operation.
- delete returns `-ENOENT` when no value exists.
- all backend operations are serialized by the service.
- empty, absolute, trailing-slash, and double-slash paths are rejected.

Namespaces belong to services. A service must not read, rewrite, or delete
another service's namespace.

## 3. AT migration

The LoRaWAN AT package continues using the existing `rzi/<key>` paths, so
firmware containing previously stored DEVEUI, APPEUI, APPKEY, BAND, CFM, and
JOIN settings remains compatible. Only the code owner changed: AT now consumes
the RZI Storage API rather than Zephyr settings directly.

## 4. Security and future evolution

The current API stores opaque bytes; it does not claim encryption, secure-key
storage, transactions, or schema migration. Production root keys should
eventually use a provisioning or secure-storage backend appropriate to the
target.

Before a stable 1.0 release, storage evolution must add:

1. schema version metadata;
2. transactional multi-key updates where required;
3. migration and rollback tests;
4. coordinated factory-reset policy;
5. a secure credential-storage profile.
