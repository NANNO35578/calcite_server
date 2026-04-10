git init
git remote add origin https://github.com/calcite-ai/calcite-server.git
git pull origin main



# MySQL database init & create tables

git add .
git commit -m "init && DB & API design"
git push -u origin main


# prepare evn
# ----------------------------------------------------- install Drogon
sudo apt install libjsoncpp-dev uuid-dev openssl libssl-dev zlib1g-dev
git clone https://github.com/drogonframework/drogon
cd drogon
git submodule update --init
mkdir build && cd build
cmake ..
make && sudo make install
# verify install
drogon_ctl version

# create project
drogon_ctl create project sources
cd sources
mkdir controllers services utils
# 创建 AuthController
drogon_ctl create controller AuthController
# 创建 NoteController
drogon_ctl create controller NoteController

# 创建模型类                  #           ###########
# 创建模型类            ############      #   #    #
# 创建模型类            #   ###    #      #  ###   #
# 创建模型类 ###########     #           #   #     #
# 创建模型类             |   ###         #  ###   #
# 创建模型类            /\___________   #    #    #
# 创建模型类
# 不要让AI写模型类了, 多多少少有问题, 直接用drogon_ctl生成, 更贴合Drogon的ORM设计
cd models
drogon_ctl create model .

# git add & commit
git add .
git commit -m "backend Note CUD APIs & Auth APIs"

git add .
git commit -m "updt docs"
git push origin main


# ----------------------------------------------------- Elastic Search
cd ~
wget https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-8.12.2-linux-x86_64.tar.gz
tar -xzf elasticsearch-8.12.2-linux-x86_64.tar.gz
cd elasticsearch-8.12.2

vim config/elasticsearch.yml
# network.host: 0.0.0.0
# http.port: 9200
# 
# discovery.type: single-node
# 
# xpack.security.enabled: false

# download kibana 8.12.2
wget https://artifacts.elastic.co/downloads/kibana/kibana-8.12.2-linux-x86_64.tar.gz
tar -zxvf kibana-8.12.2-linux-x86_64.tar.gz
cd kibana-8.12.2

# add ES Index
# curl -X PUT "localhost:9200/notes" -H 'Content-Type: application/json' -d '
# {
#   "mappings": {
#     "properties": {
#       "note_id": { "type": "long" },
#       "user_id": { "type": "long" },
#       "title": { "type": "text" },
#       "content": { "type": "text" },
#       "tags": { "type": "keyword" },
#       "created_at": { "type": "date" },
#       "updated_at": { "type": "date" }
#     }
#   }
# }
# '
# out: {"acknowledged":true,"shards_acknowledged":true,"index":"notes"}
#
# Actully, Index在代码里创建了，这里就不需要了，直接往里面插入数据就行了
#
curl -X POST "localhost:9200/notes/_doc/1" -H 'Content-Type: application/json' -d '{
  "note_id": 1,
  "user_id": 100,
  "title": "第一篇笔记",
  "content": "Kibana查看Elasticsearch数据测试",
  "tags": ["kibana","es","教程"],
  "created_at": "2026-04-02T22:00:00Z",
  "updated_at": "2026-04-02T22:00:00Z"
}'

curl -X POST "localhost:9200/notes/_doc/2" -H 'Content-Type: application/json' -d '{
  "note_id": 2,
  "user_id": 100,
  "title": "C++连接ES",
  "content": "用Drogon封装EsClient",
  "tags": ["cpp","es","drogon"],
  "created_at": "2026-04-02T23:00:00Z",
  "updated_at": "2026-04-02T23:00:00Z"
}'
# out:{"_index":"notes","_id":"1","_version":1,"result":"created","_shards":{"total":2,"successful":1,"failed":0},"_seq_no":0,"_primary_term":1}
#     {"_index":"notes","_id":"2","_version":1,"result":"created","_shards":{"total":2,"successful":1,"failed":0},"_seq_no":1,"_primary_term":1}

# Web端可视化软件Kibana 需要安装相同版本!!!

# ------------------------------------------------- install docker
# 卸载旧版本（如果有）
sudo apt remove docker docker.io containerd runc -y
sudo apt autoremove -y
sudo apt clean

# 更新依赖
sudo apt update
sudo apt install -y ca-certificates curl gnupg

# 添加 Docker 官方 GPG
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/debian/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# 添加 Debian 专用源（关键！）
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/debian \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# 更新源
sudo apt update

# 安装 Docker（现在绝对能装上）
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# 免 sudo
sudo usermod -aG docker $USER
newgrp docker


# 创建 / 编辑 docker 配置文件
sudo mkdir -p /etc/docker
sudo nvim /etc/docker/daemon.json
# 换源
{
  "registry-mirrors": [
    "https://docker.1ms.run",
    "https://docker.mirrors.ustc.edu.cn",
    "https://hub-mirror.c.163.com",
    "https://mirror.baidubce.com"
  ]
}
# 重启docker
sudo systemctl daemon-reload
sudo systemctl restart docker
# 一键脚本
# curl -fsSL https://get.docker.com -o get-docker.sh
# sudo sh get-docker.sh

# ---------------------------------------------------------- MinIO

# run MinIO
# 拉取镜像
docker pull minio/minio
# 启动 MinIO
docker run -d \
  -p 9000:9000 \
  -p 9001:9001 \
  --name minio \
  -e MINIO_ROOT_USER=admin \
  -e MINIO_ROOT_PASSWORD=12345678 \
  -v ~/github/calcite_services/minio:/data \
  minio/minio server /data --console-address ":9001"

# 删掉旧的错误容器; 因为未使用Root
# docker rm -f minio
# 电脑重启用 docker start minio



# ---------------------------------------------------------- PaddleOCR
# PaddleOCR

# 1️⃣ 环境准备（WSL）

sudo apt update
sudo apt install python3-pip -y


## 2️⃣ 安装 PaddleOCR
sudo apt install python3.11-venv
python3 -m venv paddleenv
source paddleenv/bin/activate
# pip install paddlepaddle -i https://mirror.baidu.com/pypi/simple
# 安装 PaddlePaddle（用官方可用源）
# pip install paddlepaddle -i https://pypi.org/simple
# pip install paddleocr
# 装错了, CPU的模型不好跑
# pip uninstall -y paddlepaddle paddlepaddle-gpu paddleocr
# pip install paddlepaddle-gpu==2.6.1 --index-url https://www.paddlepaddle.org.cn/packages/stable/cuda122/
# pip install paddleocr -i https://mirrors.aliyun.com/pypi/simple/

## 3️⃣ 测试 OCR 是否可用
# paddleocr ocr --image_dir image_test.png --use_angle_cls true --lang ch
# paddleocr ocr -i image_test.png --lang ch > result.txt 2>&1

# 4️⃣ 下载 v5_server 模型（关键）
# mkdir -p ~/models/paddleocr
# cd ~/models/paddleocr
# 
# wget https://paddleocr.bj.bcebos.com/PP-OCRv5/chinese/ch_PP-OCRv5_server_infer.tar
# tar -xvf ch_PP-OCRv5_server_infer.tar


# 以上方法均失败

# 直接使用在线OCR
# 直接使用在线OCR
# 直接使用在线OCR
# 直接使用在线OCR
# 直接使用在线OCR
# 直接使用在线OCR
# 直接使用在线OCR

# Lambda 太难调了
gdb calcite 2>&1 | tee out.log