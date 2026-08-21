# OpenWrt for MT6990 / FiberHome LG6851F

本仓库是基于 OpenWrt SNAPSHOT 的完整源码树，增加 MediaTek MT6990 和烽火 Pro LG6851F 支持。仓库目标是在干净 Linux 环境中克隆后，使用 OpenWrt 原生 `make` 流程生成完整可刷写文件，不需要另行调用外部打包脚本。

## 已支持功能

- MT6990 Linux 6.18 内核、时钟、PCIe、以太网、SGMII、CCCI/5G 模组等设备支持
- FiberHome LG6851F 设备树和 B 槽启动布局
- MT7996/MT7990 Wi-Fi 支持
- LuCI 5G 模组管理
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
boot和rootfs签名成功包/
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

后续 MT6990 第三方软件将单独维护为 feed 仓库，避免主源码树被不同用户的插件需求污染。

## 2026-08-21 维修固化说明

- OpenClash 使用 Mihomo 时，核心包装器必须以 `/tmp/clash` 为运行文件名。OpenClash 的服务与防火墙脚本通过 `pidof clash` 检测核心；使用其他文件名会导致核心已经监听端口却被误判为启动失败。修复包含在 `mihomo-openclash-core 1.19.29-r2`。
- `mt6990-userdata-storage 1-r3` 支持从当前根 overlay 的真实 `upperdir` 迁移数据，不再假定源目录恒为 `/overlay/upper`。
- LG6851F Web 升级保留设置时，preinit 仅对带 `.mt6990-extroot-managed` 标记的软件层刷新新 squashfs UUID 绑定，使 `/dev/mmcblk0p46` 的软件、管理员密码和 Wi-Fi 配置在升级后继续挂载；该流程不格式化分区、不修改 GPT，也不触碰 A 槽。
- 本次修复保持 Linux 6.18.44，不包含内核版本升级。

## 安全说明

仓库不包含设备分区备份、SSH 密钥、用户账号、维修现场日志和崩溃证据。构建链内的 MTK `hsm_test_keys` 仅用于当前已验证的测试签名流程，不应用于生产密钥管理。

## 上游与许可证

本项目基于 [OpenWrt](https://github.com/openwrt/openwrt)。OpenWrt 主体代码使用 GPL-2.0，各组件以其源文件中标注的许可证为准。
