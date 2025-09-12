
## 如何构建镜像
前提：安装好yocto相关构建环境

### 1.安装 repo 工具
```bash
mkdir -p ~/.bin
PATH="${HOME}/.bin:${PATH}"
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/.bin/repo
chmod a+x ~/.bin/repo
```

### 2.gitclone 我的项目

### 3.获取 BSP 源码

# 确保您当前在 openigw-project 目录下
```bash
# 确保您当前在 openigw-project 目录下
# 使用 repo 初始化 NXP BSP
repo init -u https://github.com/nxp-imx/imx-manifest -b imx-linux-kirkstone -m imx-5.15.71-2.2.0.xml

# 开始同步源码，这将花费很长时间
repo sync
```

