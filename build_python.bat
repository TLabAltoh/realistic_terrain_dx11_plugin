rm -rf pybind11*
rm -rf Python-3.10.8.tgz
rm -rf Python-3.10.8

clear

git clone https://github.com/pybind/pybind11.git
curl -O https://www.python.org/ftp/python/3.10.8/Python-3.10.8.tgz

tar -xvzf Python-3.10.8.tgz

cd Python-3.10.8
PCbuild\build.bat
