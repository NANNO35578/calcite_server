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