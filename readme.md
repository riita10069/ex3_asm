## ex3_asm
東工大情報通信実験３で使用する教育用アセンブリであるex3_asmの実行環境を
実験室のパソコンではなく、ローカルで実行するためのものです。
実験室に接続せずにasmファイルを動かしたい場合に使ってください。

- WSL
- Docker for MacまたはDocker for Windows

上記のいずれかを使用することを想定しています。

### WSL
`sh run_wsl.sh`

### Docker
`sh run_docker.sh`

## ファイルの実行方法

### WSL
`./ex3_asm filename.asm`

### Docker
`docker run -it {image_name} -v ./asm:. /bin/bash`
`./ex3_asm filename.asm`


正しく環境ができているかを確かめるために、
test.asmを用意しています。

`./ex3_asm test.asm`

