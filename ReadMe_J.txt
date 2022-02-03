B5L Sample Code (C++)



0. �͂��߂�
����Sample Code�́AOMRON����ToF�Z���T�AB5L�V���[�Y�ƒʐM���p�����[�^�̊m�F�E�ݒ�A�����摜�̎擾���s���܂��B
Interactive Mode��Non-Interactive Mode������A��҂ł͎w�肳�ꂽ�����Ŏ����I�Ɍv�����s���܂��B
�{Sample Code��C++/C����ŏ�����Ă���ALinux(Raspberry Pi OS Buster, Ubuntu 20.04)�y��Windows10�œ���m�F�����Ă��܂��B

1. �t�@�C�������ƃr���h���@
�{Sample Code�ɂ͈ȉ��̃t�@�C�����܂܂�Ă��܂��B

   build.sh             [   Linux   ] �R���p�C���p�V�F���X�N���v�g
   connect_tof.sh       [   Linux   ] ToF�ڑ��p�V�F���X�N���v�g(ToF_Sample���s�ł��܂������Ȃ��Ƃ��Ɏg�p���Ă�������)
   DetectionCuboid.pdf  [ Win/Linux ] ���o�̈�ݒ��̐����}
   init_gpio7.sh        [RaspberryPi] GPIO 7�ԃs���̏o�͐ݒ�p�V�F���X�N���v�g
   ToF_Sample.cpp       [ Win/Linux ] �T���v���R�[�h�{��
   ToF_Sample.prm       [ Win/Linux ] �p�����[�^�ݒ�t�@�C��
   ToF_Sample.sln       [  Windows  ] Visual Studio 2019�p�\�����[�V�����t�@�C��
   ToF_Sample.vcxproj   [  Windows  ] Visual Studio 2019�p�v���W�F�N�g�t�@�C��
   TOFApiZ.cpp          [ Win/Linux ] B5L�R�}���h�ʐM�R�[�h
   TOFApiZ.h            [ Win/Linux ] B5L�R�}���h�ʐM�R�[�h�w�b�_
   uart.h               [ Win/Linux ] �V���A���ʐM�p�R�[�h�w�b�_
   uart_linux.c         [   Linux   ] �V���A���ʐM�p�R�[�h
   uart_windows.c       [  Windows  ] �V���A���ʐM�p�R�[�h

Linux�ł́A�Y�t��build.sh�����s���邱�ƂŎ��s�t�@�C��ToF_Sample���쐬����܂��B
Windows10�ł́AVisual Studio 2019��ToF_Sample.sln���J��Build���邱�Ƃ��ł��܂��B

2. Interactive Mode
Interactive Mode�ł́AB5L�̃R�}���h���P���蓮�Ŕ��s���ē�����m�F���邱�Ƃ��ł��܂��B
�R�}���h��I������ƁA�p�����[�^�������Ȃ��R�}���h�ł���Α����Ɏ��s����܂��B
���ʏo�͂𔺂��ꍇ�͊i�[����t�@�C���������߂��A������w�肷��Ǝ��s����܂��B�w�肵�Ȃ��ꍇ�͎��s�͒��~����܂��B
�p�����[�^��ύX����R�}���h�́A�܂����݂̐ݒ�l���\������ύX����l�����߂��܂��B�l����͂���ΕύX����A���Ȃ���Β��~����܂��B


3. Non-Interactive Mode
Non-Interactive Mode�ł́A�p�����[�^�t�@�C���̐ݒ�l�Ŏw�肵���t���[����(���邢��Ctrl+C�Œ�~�����܂�)�̑������s���A���ʂ��t�@�C���ɏo�͂��܂��B
�܂����o�̈悪�w�肳��Ă����ꍇ�A�������������ꂽ�ۂɎw�肵���R�}���h�����s����܂�(6-5.���o�̈�@�\�Q��)�B
���̋@�\�́A�ړ����{�b�g�ɓ��ڂ����ۂ̏�Q�����m���C���[�W���Ă��܂��B


4. �p�����[�^�t�@�C��
Non-Interactive Mode�ł́A�����Ńp�����[�^�t�@�C�����w�肷��K�v������܂��B
���̓��e�ɏ]����B5L�̃p�����[�^������������܂��B
�܂��A�o�̓t�H���_�̎w��Ȃ�Sample Code�̓�����ݒ肵�܂��B
�p�����[�^�̓��e�ɂ��ẮA�t�@�C���̃R�����g��B5L��User's Manual���Q�Ƃ��Ă��������B
�p�����[�^�͎w�肪����x��B5L�ɐݒ肳��邽�߁A�����p�����[�^�𕡐��w�肵���ꍇ�ォ��w�肵�����̂��L���ɂȂ�܂��B
�w�肪�Ȃ��p�����[�^�͍X�V����܂���B�o�̓t�H���_�̎w�肪�Ȃ��ꍇ�̓t�@�C���o�͂���܂���B�J�����g�f�B���N�g���ɏo�͂������ꍇ�́A"."���w�肵�Ă��������B


5. Sample Code�Ǝ��̏����ɂ���
�{Sample Code�͊�{�I��B5L�̋@�\�����̂܂�C++/C���痘�p�ł���悤�ɂ��邱�Ƃ����Ƃ��Ă��܂����A�������ǉ��̋@�\�Ȃǂ�����܂��B

5-1. BMP File�o��
�ɍ��W�`���̋����f�[�^�ƐU���f�[�^�́AB5L�̏o�͂��̂܂܂����ł͂Ȃ�BMP�t�@�C���`���ɕϊ��������̂��o�͂��܂��B
�ɍ��W�`���̋����f�[�^�́A�ߋ�����Ԃ�����������\�������J���[BMP�摜�ɕϊ�����܂��B
�܂��A������.dpt�t�@�C���Ƃ���B5L�̏o�͂��������f�[�^�����̂܂܏o�͂���܂��B
�U���f�[�^�̏ꍇ��0�`255�̃O���[BMP�摜�ɕϊ�����܂��B
�������BMP�f�[�^��Low Amplitude�͍��AOverflow��Satulation�͔��ɂȂ�܂��B

5-2. PCD�摜�`���̕ϊ��ɂ���(float���Axyzi)
PCD�摜�o�͂ƂȂ钼�����W�`���̏ꍇ�A�g����������P���邽��B5L�̕W���t�H�[�}�b�g����2�_�ύX���Ă��܂��B
�e���W�l�́A�W����2Bytes Integer����8Bytes Float�ɕϊ�����܂��B
�܂��AAmplitude�f�[�^���o�͂���郂�[�h�ł́Axyz�t�H�[�}�b�g�ł͂Ȃ��Axyzi�t�H�[�}�b�g�ŏo�͂���܂��B
���̍ۂ�i�t�B�[���h�́AAmplitude�f�[�^���������ɂ����1m�ʒu�ł�1 Byte�l�Ɋ��Z�������̂ƂȂ�܂��B

5-3. RAW Data�o��
ToF_Sample.cpp��RAW_OUTPUT��1�ƒ�`����ƁA���ׂĂ̌��ʎ擾�t�H�[�}�b�g��B5L�̃o�C�i���o�͂����̂܂�.raw�t�@�C���Ƃ��ďo�͂���܂��B
0�ƒ�`���邱�ƂŁA��L�̂悤��BMP�EPCD�t�@�C���o�͂ƂȂ�܂��B
RAW Data�o�͂̏ꍇ�́A�ȉ��̗L������͈͊O�}�X�N�@�\�E���o�̈�@�\�͓����܂���B

5-4. �L������͈͊O�}�X�N�@�\(Non-Interactive Mode�̂�)
�p�����[�^�t�@�C����FOV_LIMITATION=1���w�肷��ƁA�����f�[�^(PCD�f�[�^)�o�͂��王��O�̉�f��������܂��B
�܂����̏ꍇ�A�o�͂����PCD�f�[�^�̃t�H�[�}�b�g��Unorganized Datasets�ƂȂ�܂��B

5-5. ���o�̈�@�\(Non-Interactive Mode�̂�)
�p�����[�^�t�@�C����DETECTION_CUBOID���w�肷�邱�Ƃɂ��A�������W�`����PCD�t�@�C�����o�͂����ꍇ�ɁA�w�肳�ꂽ�����̂̓����Ɋ܂܂��_�̐���臒l�Ɣ�r���ăR�}���h�����s���邱�Ƃ��ł��܂��B
�̈��0�`9�̍ő�10���w�肷�邱�Ƃ��ł��A�����ԍ��𕡐���w�肷��ƌォ��w�肵�����̂��L���ɂȂ�܂��B
�̈��2�_��XYZ���W�Ŏw�肵�AX�EY�EZ���ɐ�����6�ʂō\������钼���̂��w�肳��܂��B
���̒��Ɋ܂܂��_�̐���臒l�ȏ�̏ꍇ(臒l�����̏ꍇ)�܂��͖����̏ꍇ(臒l�����̏ꍇ)�A�w�肵���R�}���h�����s����܂��B臒l��0�̏ꍇ�́A�ݒ�͖����ƂȂ�܂��B
FOV_LIMITATION���L���ȏꍇ�A����O�̉�f�͌v���Ɋ܂܂�܂���B
�Ȃ��A�O�q�̂悤�ɏo�̓t�H���_�̎w������Ȃ��ꍇ�A�摜�t�@�C���͏o�͂���܂���B
�{�@�\�̂ݎg�p����ꍇ�Ɋ��p���������B


[�g�p��̒���]
* �{�T���v���R�[�h����уh�L�������g�̒��쌠�̓I�������ɋA�����܂��B
* �{�T���v���R�[�h�͓����ۏ؂�����̂ł͂���܂���B
* �{�T���v���R�[�h�́AApache License 2.0�ɂĒ񋟂��Ă��܂��B

----
�I�������������
Copyright 2019-2022 OMRON Corporation�AAll Rights Reserved.
