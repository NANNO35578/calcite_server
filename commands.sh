git init
git remote add origin https://github.com/calcite-ai/calcite-server.git
git pull origin main



# MySQL database init & create tables

git add .
git commit -m "init && DB & API design"
git push -u origin main


# prepare evn
# install Drogon
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
cd models
drogon_ctl create model .

# git add & commit
git add .
git commit -m "backend Note CUD APIs & Auth APIs"

git add .
git commit -m "updt docs"
git push origin main


# Elastic Search
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
curl -X PUT "localhost:9200/notes" -H 'Content-Type: application/json' -d '
{
  "mappings": {
    "properties": {
      "note_id": { "type": "long" },
      "user_id": { "type": "long" },
      "title": { "type": "text" },
      "content": { "type": "text" },
      "tags": { "type": "keyword" },
      "created_at": { "type": "date" },
      "updated_at": { "type": "date" }
    }
  }
}
'
# out: {"acknowledged":true,"shards_acknowledged":true,"index":"notes"}

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