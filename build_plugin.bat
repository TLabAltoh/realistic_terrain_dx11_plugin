clear
rm -rf build

cd Python-3.10.8
mv PC\pyconfig.h Include
cd ..

mkdir build
cd build

cmake ..
cmake --build .

cd ..
