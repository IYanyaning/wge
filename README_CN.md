# WGE
<p align="center">
    <img src="https://avatars.githubusercontent.com/u/215498892?s=400&u=8e3433ea2368169c950ca497194776511c8a1837&v=4" width=200 height=200/>
</p>

[简体中文] | [English](README.md)

## 什么是 WGE？
WGE 是一个基于 C++ 的高性能 Web 应用防火墙（WAF）库。它已成功应用于商业产品：石犀 - Web 治理引擎（SR-WGE）。WGE 旨在完全兼容 OWASP 核心规则集（CRS），并可作为 ModSecurity 的直接替代品。该库采用 C++23 构建，设计目标是快速、高效且易于使用。

## 性能对比
处理器: Intel(R) Core(TM) i5-10400 CPU @ 2.90GHz   2.90 GHz  
内存: 32GB  
操作系统: Ubuntu 20.04.6 LTS (5.15.153.1-microsoft-standard-WSL2)  
Worker Thread: 8  
测试样本: [白样本](benchmarks/test_data/white.data) 以及 [黑样本](benchmarks/test_data/black.data)

| 规则集         | 内存池(TCMalloc) |ModSecurity |    WGE     |
|-------------------|------------------------------|------------|------------|
| CRS v4.3.0        |         关闭                   | 4010 QPS   | 17560 QPS  |
| CRS v4.3.0        |         开启                  | 4927 QPS   | 18864 QPS  |  


基准性能测试结果显示，WGE 的性能显著优于 ModSecurity，提升幅度超过 4 倍。这得益于现代 C++ 特性的应用以及出色的架构设计与实现。WGE 旨在易于使用并能无缝集成到现有应用程序中，是开发者为项目添加 WAF 功能的理想选择。

## 快速开始
### 依赖环境
* CMake 3.28 或更高版本 https://cmake.org/download/
* vcpkg (使用 cmake 安装) https://github.com/microsoft/vcpkg
* 支持 C++23 的编译器 (GCC 13.1 或更高版本) https://gcc.gnu.org/
* Ragel 6.10
```shell
apt install ragel
```
* JDK 21 或更高版本
```shell
apt install openjdk-21-jdk-headless
```
* ANTLR4 4.13.2 或更高版本
```shell
cd /usr/local/lib
curl -O https://www.antlr.org/download/antlr-4.13.2-complete.jar
```
* 配置环境变量。将以下内容添加到 `/etc/profile`：
```shell
export CLASSPATH=".:/usr/local/lib/antlr-4.13.2-complete.jar:$CLASSPATH"
alias antlr4='java -Xmx500M -cp "/usr/local/lib/antlr-4.13.2-complete.jar:$CLASSPATH" org.antlr.v4.Tool'
alias grun='java -Xmx500M -cp "/usr/local/lib/antlr-4.13.2-complete.jar:$CLASSPATH" org.antlr.v4.gui.TestRig'
```
### 构建
* 更新子模块
```shell
git submodule update --init
```
* 配置 cmake
```shell
cmake --preset=release-with-debug-info --fresh
```
如果编译器路径不在默认路径中，我们可以将 `CMakeUserPresets.json.example` 复制为 `CMakeUserPresets.json`，并在该文件中修改编译器路径，例如：
```json
{
    "name": "my-release-with-debug-info",
    "inherits": "release-with-debug-info",
    "environment": {
    "CC": "/usr/local/bin/gcc",
    "CXX": "/usr/local/bin/g++",
    "LD_LIBRARY_PATH": "/usr/local/lib64"
    }
}
```
然后我们可以运行 cmake 命令：
```shell
cmake --preset=my-release-with-debug-info --fresh
```
如果我们想启用调试日志以便观察 WGE 的处理过程，可以将 `WGE_LOG_ACTIVE_LEVEL` 设置为 1。
```shell
cmake --preset=release-with-debug-info --fresh -DWGE_LOG_ACTIVE_LEVEL=1
```
`WGE_LOG_ACTIVE_LEVEL` 是一个编译期的选项，该宏用于控制日志级别：
1: Trace  
2: Debug  
3: Info  
4: Warn  
5: Error  
6: Critical  
7: Off 
* 使用 cmake 构建
```shell
cmake --build build/release-with-debug-info
```
### 运行单元测试
```shell
./build/release-with-debug-info/test/test
```
### 运行基准性能测试
```shell
./build/release-with-debug-info/benchmarks/wge/wge_benchmark
```
### 集成到现有项目
* 安装 WGE
```shell
cmake --install build/release-with-debug-info
```
安装完成后，WGE 库和头文件将可在系统的包含路径和库路径中使用。我们也可以通过指定 `--prefix` 选项将 WGE 安装到其他路径。例如，要将 WGE 安装到 `/specified/path`，我们可以运行：
```shell
cmake --install build/release-with-debug-info --prefix /specified/path
```
* 在现有项目中包含 WGE 的头文件
```cpp
#include <wge/engine.h>
```
* 在现有项目中链接 WGE
```cmake
# 如果 WGE 安装在系统路径中
target_link_libraries(your_target_name PRIVATE wge)
# 如果 WGE 安装在指定路径中
target_link_libraries(your_target_name PRIVATE /specified/path/lib/libwge.a)
```
* 在现有项目中使用 WGE
1. 在主线程中构造 WGE 引擎实例
```cpp
Wge::Engine engine(spdlog::level::off);
```
2. 在主线程中加载规则文件
```cpp
std::expected<bool, std::string> result = engine.loadFromFile(rule_file);
if (!result.has_value()) {
  // Handle the error
  std::cout << "Load rules error: " << result.error() << std::endl;
}
```
3. 在主线程中初始化引擎
```cpp
engine.init();
``` 
4. 在工作线程中为每个请求创建事务
```cpp
// 每个请求都有自己的事务
Wge::TransactionPtr t = engine.makeTransaction();
```
5. 在工作线程中处理请求
```cpp
// 处理每个事务的步骤如下
// 1. 处理连接
t->processConnection(/*params*/);
// 2. 处理 URI
t->processUri(/*params*/);
// 3. 处理请求头
t->processRequestHeaders(/*params*/);
// 4. 处理请求体
t->processRequestBody(/*params*/);
// 5. 处理响应头
t->processResponseHeaders(/*params*/);
// 6. 处理响应体
t->processResponseBody(/*params*/);
```

请参考 [wge_benchmark](benchmarks/wge/main.cc) 获取使用示例。

## 开源许可证
版权所有 (c) 2024-2026 石犀及其贡献者。
WGE 采用 MIT 许可证发布。有关完整详情，请参阅随附的 [开源许可证](LICENSE) 文件。

## 文档
我们正在编写 WGE 的文档。在此期间，请参考相关项目中的源代码和示例，以了解如何使用该库。
### 相关项目
* [WGE-Connectors](https://github.com/stone-rhino/wge-connectors): 用于将 WGE 集成到各种 Web 服务器和框架中的连接器集合。

## 贡献
我们欢迎对 WGE 的贡献！如果您有任何想法、建议或错误报告，请在 GitHub 上打开问题或提交拉取请求。在贡献之前，请阅读我们的 [贡献指南](CONTRIBUTING.md)。

## 联系方式
[石犀](https://www.srhino.com/)