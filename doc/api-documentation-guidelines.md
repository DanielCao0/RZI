# RZI 公共 API 注解与文档规范

## 1. 适用范围

本规范适用于 `include/rzi/` 和 `src/` 下所有 RZI 生产库 C 源文件及头文件。
公共 C API 包括函数、callback、结构体、枚举、常量和公开类型；新增或修改公共
API 时必须同时更新对应 Doxygen 注解。

公共头文件使用 Zephyr 风格 Doxygen，并通过 Zephyr toolchain attributes 提供
编译期检查。每个生产库 `.c` 和 `.h` 文件均必须包含 `@file` 和 `@brief`
文件级注解。私有 contract、跨文件类型和跨文件函数使用 Doxygen 记录；私有
实现可以使用普通注释解释设计原因，不要求为每个 `static` 函数机械地编写
Doxygen。`samples/` 和 `tests/` 不属于生产库文件，不强制使用文件级 Doxygen。

## 2. 文件和 API group

每个生产库 C 源文件和头文件必须包含：

```c
/**
 * @file
 * @brief One-line file purpose.
 */
```

每个公共头文件还必须加入一个 service group。每个 service 必须定义
Doxygen group，并记录首次公开版本和当前 API 版本：

```c
/**
 * @defgroup rzi_service RZI service API
 * @brief One-line service purpose.
 * @since 0.2
 * @version 0.2.0
 * @{
 */
```

子模块使用 `@ingroup` 加入所属 service。文件末尾必须使用 `/** @} */` 关闭
group。私有头文件和 `.c` 文件不定义公共 group，避免把内部符号混入稳定 API
参考手册。

`scripts/check-style.sh` 会扫描 `include/rzi/` 和 `src/`，任何缺少 `@file`
或 `@brief` 的生产库文件都会导致检查失败。

## 3. 公共函数

每个公共函数必须记录：

- `@brief`：一句话说明操作；
- `@param`：每个参数的方向、有效范围、所有权和生命周期；
- `@return` 或 `@retval`：完整返回语义；
- thread context：是否允许 ISR、callback 或并发调用；
- asynchronous semantics：返回 `0` 是“完成”还是“请求已受理”；
- 数据是否在返回前复制。

已知且有限的返回值使用多个 `@retval`。返回值是范围或无法穷举时使用
`@return`。同一函数不应机械地重复两种标签。

示例：

```c
/**
 * @brief Request LoRaWAN network activation.
 *
 * The configuration is copied before this function returns. A return value of
 * zero means accepted; completion is reported through join_done().
 *
 * @param config Activation configuration.
 *
 * @retval 0 Request accepted.
 * @retval -EINVAL Invalid configuration.
 * @retval -EAGAIN Backend not ready.
 * @retval -ENOTSUP Activation mode unsupported.
 *
 * @note Thread context only; this function must not be called from an ISR.
 * @since 0.2
 */
__must_check int rzi_lorawan_join(
	const struct rzi_lorawan_join_config *config);
```

## 4. 约束注解

- `@note`：线程上下文、数据生命周期、异步行为等正常使用条件；
- `@warning`：忽略后可能造成数据丢失、安全风险或不可逆结果的条件；
- `@pre`：调用者必须在调用前满足且 API 无法自行建立的前置条件。

不要用 `@warning` 强调普通信息。不要只写“成功返回 0”，而遗漏具体错误路径。

callback 必须明确：

- 执行线程；
- 是否持有内部锁；
- 是否允许重入 RZI；
- 指针参数的有效期；
- callback 是否允许阻塞。

## 5. 版本和废弃

- 新增公共 group 必须添加 `@since`；group 成员默认继承该版本；
- 在后续版本新增的类型或函数必须单独添加 `@since`；
- group 使用 `@version` 表示该 API 集合的当前语义版本；
- 废弃 API 同时使用 Doxygen `@deprecated` 和编译器 `__deprecated`；
- `@deprecated` 必须给出替代接口和计划移除的 major version；
- 废弃 API 至少保留一个正常发布周期，除非 RZI 尚未承诺稳定 ABI。

示例：

```c
/**
 * @deprecated Since 0.3; use rzi_service_start(). Removed in 1.0.
 */
__deprecated int rzi_service_init(void);
```

## 6. 编译器检查

公共头文件从 `<zephyr/toolchain.h>` 使用跨编译器 attribute macros。

### `__must_check`

返回错误码、handle 或调用者必须消费结果的函数必须标记 `__must_check`：

```c
__must_check int rzi_service_start(void);
```

GCC 的 `warn_unused_result` 即使使用 `(void)` 转换仍可能告警。调用者应处理
错误；RZI 私有实现确实无法处理某个结果时，必须通过命名明确的 helper 消费，
使 code review 能识别这是有意行为：

```c
static void ignore_result(int result)
{
    ARG_UNUSED(result);
}

ignore_result(rzi_service_stop());
```

纯查询且忽略结果没有副作用的函数不强制使用 `__must_check`。

### `__printf_like(format_index, first_arg_index)`

所有 printf-style variadic API 必须标记格式参数和首个可变参数位置：

```c
__printf_like(1, 2)
__must_check int rzi_at_publish_event(const char *format, ...);
```

索引从 1 开始。`va_list` 版本的第二个参数使用 `0`。

### `__deprecated`

只有已经进入正式废弃流程的公共 API 才能使用 `__deprecated`。不得为了标记
“暂时不推荐”而使用它。

## 7. 不使用 `__syscall`

`__syscall` 是 Zephyr userspace 系统调用声明，不是通用文档注解。它需要
`z_impl_*` 实现、`z_vrfy_*` 参数和权限校验以及 syscall metadata。

RZI 当前公共 API 运行在 supervisor mode，不得添加 `__syscall`。未来只有在
明确支持 `CONFIG_USERSPACE=y`、完成对象权限和用户内存验证后，才能为经过
安全评审的接口增加该声明。

## 8. Review checklist

提交公共 API 修改前必须确认：

- [ ] 所有生产库 `.c/.h` 文件都包含 `@file` 和 `@brief`；
- [ ] 公共头文件的 service group 包含 `@brief`、`@since`、`@version`；
- [ ] 所有参数都有范围、方向、所有权和生命周期说明；
- [ ] 同步返回值与异步完成结果没有混淆；
- [ ] 公开错误码均有 `@retval` 或统一 `@return` 说明；
- [ ] thread/ISR/callback context 已说明；
- [ ] 临时指针的有效期已说明；
- [ ] 必须处理的返回值使用 `__must_check`；
- [ ] variadic format API 使用正确的 `__printf_like`；
- [ ] 废弃 API 同时包含 `@deprecated` 和 `__deprecated`；
- [ ] 没有把 `__syscall` 当作普通 attribute 使用。
