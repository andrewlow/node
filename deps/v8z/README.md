v8z
=====

Port of Google V8 javascript engine to IBM z/OS.

## Compile code
Do this part on a Linux on Z machine since there is no git or svn on z/OS.
```
git clone https://github.com/ibmruntimes/v8z
cd v8z
git checkout 3.28-zos
cd ..
scp -r v8z user@machine:path
```
Do this part on the z/OS machine where you copied the repo. Ensure that xlc 2.2,
Make 4.1 and Python 2.7 are set up correctly.
```
export GYP_DEFINES="OS=os390 target_arch=s390x v8_target_arch=s390x"
export CXX=xlc
export CXX_host=$CXX
export LINK=$CXX
cd v8z
```

#### 31-bit
```
make s390 i18nsupport=off snapshot=off
```
#### 64-bit
```
make s390x i18nsupport=off snapshot=off
```

## Test code
```
// Use -j1 if multiprocessing is not supported in your z/OS Python port.
python tools/run-tests.py -j1 --no-presubmit --arch-and-mode=s390.debug
```
