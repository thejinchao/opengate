#!/usr/bin/python
#-*- coding: utf-8 -*-

#生成Java和CSharp所需要的Message文件

import os
import sys
import string
import platform

print "delete old dir and files ..."
os.system("mkdir _temp")
isLinux = (platform.uname()[0]=='Linux')
protoc = 'protoc.exe'

if (isLinux) :
	protoc = os.getenv('PROTOBUF_SDK_ROOT') + "/bin/protoc";
else:
	protoc = os.getenv('PROTOBUF_SDK_ROOT') + "/bin/protoc.exe";

print protoc

##########################java generate java Message files begin ##########################
for filename in os.listdir("./protofiles"):
	print "++++++++++++++protofile:[" + filename+"] is beginning++++++++++++++"		
	proto_file = ("./protofiles/" + filename ).replace('\n','')		
	print("proto_file:"+proto_file)
	if (isLinux) :
		os.system(protoc + " -I=./protofiles --java_out=.//_temp// "+proto_file)
	else :	
		os.system(protoc + " -I=.\protofiles --java_out=.\\_temp\\ "+proto_file)
	print "Generated success!![" +proto_file +"]"
##########################java generate java Message files begin ##########################


###-----------------------added the following at 2-14-5-4 by liguofang------------
##########################c++   generate   cs Message files begin ##########################
	print("begin generate cplus file proto_file:"+proto_file)
	if (isLinux) :
		os.system(protoc + " -I=./protofiles --cpp_out=./_temp/ "+proto_file)
	else :
		os.system(protoc + " -I=.\protofiles --cpp_out=.\\_temp\\ "+proto_file)
	print "Generated cplus files success!![" +proto_file +"]"
	
##########################cs   generate   cs Message files end   ##########################




