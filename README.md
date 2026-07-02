<div align=center>
    <img width="64" src="doc/preview/logo.png">

# AlwaysLang
可以为不同软件固定不同输入语言，自动检测系统已安装的输入语言并跟随切换，支持输入位置语言浮层显示。[去下载](https://github.com/SugarCarry/AlwaysLang/releases)

</div>


<div align=center>
   <img src="doc/preview/software.gif">
</div>

# 功能特性
* 自动检测系统已安装的输入语言，任意语言（中 / 英 / 韩 / 日 / 法 / 俄……）都可设为某个软件的固定输入语言
* 为每个软件单独设置目标输入语言，切换到对应软件时自动跟随切换
* 输入位置语言跟随浮层，在光标附近显示当前输入语言，CMD / PowerShell 经典控制台窗口也支持光标跟随
* 浮窗不透明度可调、背景颜色可自定义（默认自动感知浮窗背后画面的明暗）
* 全局快捷键开关浮窗显示（默认 `Ctrl + Shift + L`，可自定义）
* 开机静默自启动，启动后自动进入运行状态并最小化到托盘
* 托盘右键菜单可快速开关浮层显示
* 夜间模式支持跟随系统 / 浅色 / 深色

# 使用说明
1. 把软件或其快捷方式拖入列表，或点击 `添加应用` 添加
2. 为每个软件从已安装语言中选择目标语言，并设置是否启用、是否开启大写锁定
3. 点击绿色开始按钮启动跟随，切换到对应软件时会自动切换到设定的输入语言
4. 下拉列表中的语言来自 Windows 语言设置中已安装的输入法，需先在系统中安装对应语言

> 开机自启动需要管理员权限：勾选「开机自启动」时会通过任务计划程序创建一个登录时触发的静默启动任务，因此首次勾选请以管理员身份运行一次。

# 依赖
* Qt Core, Qt Quick, Qt QML, Qt ShaderTool, Qt 5 Compatibility Module. (必须)
* Qt LinguistTool (可选)
* Qt Svg (可选 但 Qt5 必须)

# 编译运行

推送到 main 分支后会由 GitHub Actions 自动构建并发布安装版与便携版，无需本地构建。如需本地编译：

1. 克隆仓库
   ~~~shell
   git clone https://github.com/SugarCarry/AlwaysLang.git
   cd AlwaysLang
   ~~~

2. 构建

    * 用 `Qt Creator` 打开根目录的 `CMakeLists.txt` 构建

    * 或用 CMake 命令构建（把 Qt 路径改成自己的）

        ~~~shell
        cmake -S . -B build -DCMAKE_PREFIX_PATH=D:\Qt\6.6.3\msvc2019_64 -DCMAKE_BUILD_TYPE=Release -GNinja
        cmake --build build --target AlwaysLang --parallel
        cmake --build build --target Script-DeployRelease
        ~~~

    * 部署完成后的软件保存在 `dist` 目录

# 致谢
* [FluentUI for qml](https://github.com/zhuzichu520/FluentUI)

# 相关资料
* [Windows 键盘布局标识符](https://www.science.co.il/language/Locale-codes.php)
* [MS-LCID](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-lcid/70feba9f-294e-491e-b6eb-56532684c37f)
