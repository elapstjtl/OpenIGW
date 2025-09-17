if [[ -n "$CONDA_DEFAULT_ENV" ]]; then
    echo "检测到conda环境: $CONDA_DEFAULT_ENV，退出..."
    conda deactivate
else
    echo "当前不在conda环境中，跳过conda deactivate"
fi

# 进入OpenIGW目录
cd ~/jtl/OpenIGW

# 设置环境变量
BUILD_DIR=~/jtl/OpenIGW/build-imx6ull-fb
if [[ -f "$BUILD_DIR/conf/local.conf" ]]; then
    echo "复用现有构建环境，不重置 conf..."
    source ./setup-environment build-imx6ull-fb
else
    MACHINE=imx6ull14x14evk DISTRO=fsl-imx-fb source ./imx-setup-release.sh -b build-imx6ull-fb
fi
# bitbake imx-image-core