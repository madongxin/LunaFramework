# LunaServer multi-role image (build once; start with role argv via entrypoint)
# 推荐：使用预装 brpc 的 toolchain 作为 base，或 build 前挂载 /usr/local。
# 亦可在 build 阶段执行 scripts/install_deps.sh --build-brpc（耗时）。
FROM centos:8 AS build
RUN sed -i 's/mirrorlist/#mirrorlist/g; s|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*.repo \
 && yum install -y gcc-c++ cmake make openssl-devel jsoncpp-devel protobuf-devel \
      mysql-devel hiredis-devel leveldb-devel gflags-devel zlib-devel \
      curl wget git \
 && yum clean all
WORKDIR /src
COPY . .
# 若镜像未带 brpc：尝试 PREFIX 构建（需网络）；已存在则跳过
RUN if [ ! -f /usr/local/include/brpc/server.h ] && [ ! -f /usr/include/brpc/server.h ]; then \
      ./scripts/install_deps.sh --build-brpc \
      && export CPATH=/root/.local/lunaserver-deps/include:${CPATH:-} \
      && export LIBRARY_PATH=/root/.local/lunaserver-deps/lib:/root/.local/lunaserver-deps/lib64:${LIBRARY_PATH:-} \
      && export LD_LIBRARY_PATH=/root/.local/lunaserver-deps/lib:/root/.local/lunaserver-deps/lib64:${LD_LIBRARY_PATH:-} \
      && ln -sfn /root/.local/lunaserver-deps/include/brpc /usr/local/include/brpc \
      && ln -sfn /root/.local/lunaserver-deps/lib*/libbrpc* /usr/local/lib64/ 2>/dev/null || true; \
    fi \
 && test -f /usr/local/include/brpc/server.h -o -f /usr/include/brpc/server.h \
 && ENABLE_BRPC=ON ./scripts/build.sh Release

FROM centos:8
RUN sed -i 's/mirrorlist/#mirrorlist/g; s|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*.repo \
 && yum install -y openssl mysql-libs hiredis protobuf gflags leveldb zlib curl \
 && yum clean all \
 && useradd -r -u 10001 -d /opt/lunaserver -s /sbin/nologin lunaserver
WORKDIR /opt/lunaserver
COPY --from=build /src/build/test/gateway /src/build/test/session /src/build/test/gamelogic \
                  /src/build/test/globalservice /src/build/test/gamedb /opt/lunaserver/
COPY config /opt/lunaserver/config
COPY deploy/docker-entrypoint.sh /opt/lunaserver/docker-entrypoint.sh
RUN chmod +x /opt/lunaserver/docker-entrypoint.sh \
 && chmod +x /opt/lunaserver/gateway /opt/lunaserver/session /opt/lunaserver/gamelogic \
             /opt/lunaserver/globalservice /opt/lunaserver/gamedb \
 && chown -R lunaserver:lunaserver /opt/lunaserver
USER lunaserver
ENV LUNASERVER_FORMAL=1 LUNASERVER_HTTP_BIND=0.0.0.0 LUNASERVER_ADVERTISE_HOST=127.0.0.1 \
    LUNASERVER_BIN_DIR=/opt/lunaserver
ENTRYPOINT ["/opt/lunaserver/docker-entrypoint.sh"]
CMD ["gateway", "8080", "8081"]
