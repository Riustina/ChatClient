# ChatClient AGENTS

## 项目属性

- 项目类型：Qt Widgets 应用
- 构建系统：qmake (`.pro`)
- 项目根目录：`D:\Workspace\Graduate\ChatClient`

## 默认交流语言

- 默认使用中文回复
- 项目中的测试数据、注释、说明文字，在不影响编码安全和编译的前提下，尽量使用中文

## 开发规则

- 使用 Qt Widgets，不要使用 QML
- 尽量使用 `.ui` 文件定义界面结构，而不是完全用代码构建 UI
- 遵循 Qt 的 signals / slots 机制
- 如果新增类，需要更新 `ChatClient.pro` 中的 `SOURCES / HEADERS / FORMS`
- 不要修改与任务无关的文件

## 工作方式

- 开始修改前，先扫描相关项目文件
- 优先在现有模块和现有结构中落地功能
- 变更完成后，列出修改/新增文件
- 变更完成后，尽量以 patch/diff 形式展示修改
- 如果条件允许，运行构建命令验证编译

## 构建工具

- qmake：`D:\Qt\6.9.0\mingw_64\bin\qmake.exe`
- make：`D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe`

## 构建前准备

在调用 `mingw32-make` 前，先确保 `PATH` 包含：

`D:\Qt\Tools\mingw1310_64\bin`

可用命令：

```powershell
$env:PATH='D:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -j1
```

## 常见问题

- 如果链接时报 `debug\ChatClient.exe: Permission denied`，通常是程序仍在运行，占用了可执行文件，先关闭程序再重新编译
- `QButtonGroup` 如果需要按 id 处理，使用 `&QButtonGroup::idClicked`，不要误写成 `QOverload<int>::of(&QButtonGroup::buttonClicked)`
