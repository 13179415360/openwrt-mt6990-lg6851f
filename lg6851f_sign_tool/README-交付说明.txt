LG6851F / MT6990 自包含签名材料

本目录是 scripts/mt6990-build-and-record.sh 生成以下三类交付物所必需的最小子集：
1. 可单刷的签名 boot_b.img
2. 可单刷的 rootfs_b.img
3. LuCI Web 可上传的 B 槽签名 sysupgrade 包

默认构建命令：
  ./scripts/mt6990-build-and-record.sh

签名脚本只使用本目录内的 MTK 工具、mt2737 配置、ROOT/IMAGE 设备签名材料
以及 reference/boot_b-reference.img，不再静默引用原开发电脑上的绝对路径。

OpenWrt 根目录的 private-key.pem 是软件包仓库签名钥匙，与 MTK boot 安全
启动签名不是同一套材料。公司内部交付时应按密钥管理制度限制本目录访问。
