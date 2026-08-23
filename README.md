# OpenWrt for MT6990 / FiberHome LG6851F

本仓库是基于 OpenWrt SNAPSHOT 的完整源码树，增加 MediaTek MT6990 和烽火 Pro LG6851F 支持。仓库目标是在干净 Linux 环境中克隆后，使用 OpenWrt 原生 `make` 流程生成完整可刷写文件，不需要另行调用外部打包脚本。

## 已支持功能

- MT6990 Linux 6.18 内核、时钟、PCIe、以太网、SGMII、CCCI/5G 模组等设备支持
- FiberHome LG6851F 设备树和 B 槽启动布局
- MT7996/MT7990 Wi-Fi 支持
- LuCI 5G 模组管理
- 已修复并实机验证 MT6990 专用 ADB USB：纯 FunctionFS ADB、通用 `18d1:4ee7` 标识，系统稳定启动 60 秒后自动启用，不创建 RNDIS/ECM/NCM USB 网卡
- 兼容新旧硬件的双通道 PWM 风扇温控
- CAKE 多用户公平性与空闲速率保护
- APK 用户态兼容软件源
- 编译完成后自动导出签名 `boot_b.img`、`rootfs_b.img` 和 LuCI Web 升级包

## 主机环境

建议使用 Ubuntu 22.04/24.04 x86_64，文件系统需区分大小写。

```bash
sudo apt update
sudo apt install -y build-essential clang flex bison g++ gawk gcc-multilib \
  gettext git libncurses-dev libssl-dev python3 python3-distutils rsync \
  unzip zlib1g-dev file wget curl swig python3-setuptools jq xsltproc \
  libelf-dev
```

Ubuntu 版本的包名如有变化，请参考 [OpenWrt Build System Setup](https://openwrt.org/docs/guide-developer/toolchain/install-buildsystem)。

## 下载与编译

```bash
git clone https://github.com/13179415360/openwrt-mt6990-lg6851f.git
cd openwrt-mt6990-lg6851f

./scripts/feeds update -a
./scripts/feeds install -a

make defconfig
make -j4 V=s
```

仓库根目录已提供 LG6851F/MT6990 默认 `.config`。如果内存较少或需要更容易定位错误，使用单线程：

```bash
make -j1 V=s
```

更新到仓库最新源码：

```bash
git pull --ff-only
./scripts/feeds update -a
./scripts/feeds install -a
make defconfig
make -j4 V=s
```

`make` 会按 OpenWrt 原生流程自动下载依赖源码、构建工具链、编译内核与软件包，然后执行 LG6851F 签名和 Web 升级包封装。

## 输出文件

主要文件位于：

```text
bin/targets/mediatek/mt6990/
A槽单刷boot和rootfs签名成功包/
B槽单刷boot和rootfs签名成功包/
```

关键产物包括：

```text
boot_b.img
rootfs_b.img
lg6851f-mt6990-b-signed-sysupgrade.tar.gz
SHA256SUMS
```

Web 升级请使用 `lg6851f-mt6990-b-signed-sysupgrade.tar.gz`。刷写前必须核对设备型号、槽位和 SHA256。

## 配置与开发

如需调整软件包：

```bash
make menuconfig
make -j4 V=s
```

常用单包编译方式：

```bash
make package/<package-name>/clean
make package/<package-name>/compile V=s
```

## MT6990 专用 APK 软件库

本项目的软件资产分为两部分：

- `main` 分支保存可重复构建的源码、MT6990 补丁和仓库生成脚本。
- [`apk-feed-6.18.44`](https://github.com/13179415360/openwrt-mt6990-lg6851f/tree/apk-feed-6.18.44) 分支保存同一套工具链编译并签名的 APK、四类 `packages.adb` 索引、公钥和 SHA256 清单。

该软件库只适用于 FiberHome LG6851F、Linux 6.18.44 和 `aarch64_cortex-a55_neon-vfpv4`。不要给其他 OpenWrt 设备使用，也不要混入官方滚动 SNAPSHOT 或通用架构软件源；内核模块尤其要求内核 ABI 完全一致。

### 固件用户如何使用

最新固件已内置公钥和软件源配置，不需要手工添加网址。为避免 GitHub Raw 在部分移动网络不可达，以及 CDN 对移动分支缓存不一致，设备固定使用经过核验的不可变二进制提交 `760d829021769a0aff5d713e7316c7d36ff40c71`：

```text
https://cdn.jsdelivr.net/gh/13179415360/openwrt-mt6990-lg6851f@760d829021769a0aff5d713e7316c7d36ff40c71/target/packages.adb
https://cdn.jsdelivr.net/gh/13179415360/openwrt-mt6990-lg6851f@760d829021769a0aff5d713e7316c7d36ff40c71/base/packages.adb
https://cdn.jsdelivr.net/gh/13179415360/openwrt-mt6990-lg6851f@760d829021769a0aff5d713e7316c7d36ff40c71/luci/packages.adb
https://cdn.jsdelivr.net/gh/13179415360/openwrt-mt6990-lg6851f@760d829021769a0aff5d713e7316c7d36ff40c71/packages/packages.adb
```

网页操作：进入 LuCI 的“系统 → 软件包”，先点“更新列表”，再搜索和安装需要的软件。

SSH 命令行操作：

```sh
# 更新四类索引
apk update

# 搜索软件
apk search -v luci-app-pwmfan

# 安装前只模拟依赖解析，不改系统
apk add --simulate luci-app-pwmfan

# 确认无冲突后正式安装
apk add luci-app-pwmfan
```

可用下面的命令检查设备是否使用专用架构和固定软件源：

```sh
cat /etc/apk/arch
cat /etc/apk/repositories.d/distfeeds.list
apk update
```

正常情况下架构应包含 `aarch64_cortex-a55_neon-vfpv4`，四个索引均指向上述固定提交。若设备仍是旧固件、Linux 版本不是6.18.44、签名公钥不匹配或源文件不存在，请先升级本项目的最新 Web 固件，不要直接复制软件源地址强行安装。

### 添加其他软件并自行编译

优先使用软件源码或 OpenWrt feed，不要把其他设备、其他架构或其他内核版本的成品 APK 直接混进本仓库。添加软件通常有两种方式。

方式一：软件已经存在于 OpenWrt feeds。先更新并安装包定义：

```bash
./scripts/feeds update -a
./scripts/feeds install -a
make menuconfig
```

在 `make menuconfig` 中搜索包名：按 `/` 后输入名称即可定位。选择方式决定产物用途：

- 选成 `<*>`：软件直接进入随后生成的 rootfs/完整 Web 固件。
- 选成 `<M>`：只构建 APK，不默认装进固件。
- `< >`：不构建。

保存配置后可以先单包验证：

```bash
make package/<包名>/clean
make package/<包名>/compile -j1 V=s
```

例如：

```bash
make package/luci-app-pwmfan/compile -j1 V=s
```

方式二：软件不在现有 feeds。把符合 OpenWrt 包规范的源码目录放到独立路径，例如：

```text
package/custom/<包名>/Makefile
package/custom/<包名>/files/
package/custom/<包名>/patches/
```

也可以在 `feeds.conf.default` 中增加自己的 Git feed，再运行 `./scripts/feeds update <feed名>` 和 `./scripts/feeds install -a`。建议个人软件放在独立 `package/custom/` 或独立 feed，不要直接改写 OpenWrt 上游同名包，便于以后更新和排查冲突。

新增包的 `Makefile` 至少要正确声明包名、版本、目标架构、依赖、源码地址与源码哈希，以及 `Build/Compile`、`Package/<名称>/install` 等实际需要的阶段。首次编译建议始终使用 `-j1 V=s`；如果失败，从日志中最早出现的 `missing dependency`、下载哈希错误或编译器错误开始处理，不要只看最后一行。

如果目的是把软件固化进 Web 升级包，将它选为 `<*>` 后执行完整构建：

```bash
make defconfig
make -j4 V=s
```

如果只想生成可在 LuCI“软件包”页面安装的 APK，将它选为 `<M>` 或 `<*>` 并完成编译，然后更新索引和专用仓库：

```bash
make package/index
./scripts/mt6990-build-apk-repository.sh
```

生成的 APK 会按来源进入 `bin/mt6990-apk-repository/target`、`base`、`luci` 或 `packages`。先检查清单和签名索引：

```bash
cat bin/mt6990-apk-repository/repository.manifest
sha256sum -c bin/mt6990-apk-repository/SHA256SUMS
```

个人发布时必须同时发布相应目录里的 APK 与 `packages.adb`，并让设备信任这次构建所用的公钥。不要发布私钥。若更换了编译配置、工具链、内核 ABI 或签名密钥，应视为新的软件库快照，使用新的固定提交或版本目录，不能覆盖旧索引后假装兼容。

### 生成完整的同批软件库

完成一次全量构建后运行：

```bash
./scripts/mt6990-build-apk-repository.sh
```

脚本会强制核对 Linux 6.18.44、目标架构、已签名的四类索引和 APK 公钥，然后把可发布目录生成到：

```text
bin/mt6990-apk-repository/
├── target/
├── base/
├── luci/
├── packages/
├── mt6990-apk-public-key.pem
├── repository.manifest
└── SHA256SUMS
```

这可确保个人软件库中的 APK 和固件来自同一次构建。APK 私钥只应保存在自己的构建机上，禁止上传到 GitHub、复制进固件或发送给他人。

## 2026-08-21 维修固化说明

- OpenClash 使用 Mihomo 时，核心包装器必须以 `/tmp/clash` 为运行文件名。OpenClash 的服务与防火墙脚本通过 `pidof clash` 检测核心；使用其他文件名会导致核心已经监听端口却被误判为启动失败。修复包含在 `mihomo-openclash-core 1.19.29-r2`。
- `mt6990-userdata-storage 1-r3` 支持从当前根 overlay 的真实 `upperdir` 迁移数据，不再假定源目录恒为 `/overlay/upper`。
- LG6851F Web 升级保留设置时，preinit 仅对带 `.mt6990-extroot-managed` 标记的软件层刷新新 squashfs UUID 绑定，使 `/dev/mmcblk0p46` 的软件、管理员密码和 Wi-Fi 配置在升级后继续挂载；该流程不格式化分区、不修改 GPT，也不触碰 A 槽。
- 本次修复保持 Linux 6.18.44，不包含内核版本升级。

## 安全说明

仓库不包含设备分区备份、SSH 密钥、用户账号、维修现场日志和崩溃证据。构建链内的 MTK `hsm_test_keys` 仅用于当前已验证的测试签名流程，不应用于生产密钥管理。

## 上游与许可证

本项目基于 [OpenWrt](https://github.com/openwrt/openwrt)。OpenWrt 主体代码使用 GPL-2.0，各组件以其源文件中标注的许可证为准。

## 设备刷机工具

- 123云盘：[下载设备刷机工具](https://4001579968.share.123pan.cn/123pan/gl1cMh-P7nP?pwd=Mvrn#)
- 访问密码：`Mvrn`
