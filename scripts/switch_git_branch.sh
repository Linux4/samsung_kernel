#!/bin/bash
# script to switch git branch/commit according to input file
# the input file format should be:
# {
# <branch name>
# <commit number>
# ...
# <commit number>
# }

ENTRIES=300
GIT_DIR=".git"
GIT_COMMAND=`which git`
if [ ! -d $GIT_DIR ];then
	echo "This shell script need to be run under folder with git"
	exit 1
fi

if [ -z $GIT_COMMAND ];then
	echo "Your system don't have git installed, please install first!!!"
	exit 1
fi

if [ -z $1 ];then
	echo "Need file as input!!!"
	exit 1
fi

if [ ! -e $1 ];then
	echo "file $1 doesn't exist!!!"
	exit 1
fi

LOG=(`cat $1`)
NUM=`echo ${#LOG[@]}`
BRANCH=${LOG[0]}

if [ $NUM -lt 2 ]; then
	echo "there is no commit at all!!!"
	exit 1
fi

for (( i=1; i < $NUM; i++ )); do
	test=`echo ${LOG[$i]} | sed 's/[0-9a-z]//g'`;
	if [ ! -z $test ]; then
		echo "Commit entry @$i is not valid:${LOG[$i]}"
		exit 1
	fi
done

COMMITS=`git log --pretty=%H -$ENTRIES $BRANCH`
START=1
LIMIT=`expr $NUM - 1`
for i in $COMMITS; do
	if [ $i = ${LOG[$START]} ];then
		if [ $START = 1 ];then
			START=2
			continue
		fi
	fi

	if [ $START -gt 1 ]; then
		if [ ! $i = ${LOG[$START]} ];then
			echo "NOT matching for the $START entry!!!"
			echo "commit find in branch:$BRANCH is $i"
			echo "commit in log file is ${LOG[$START]}"
			exit 1
		fi
		START=`expr $START + 1`
	fi

	if [ $START -ge $LIMIT ];then
		break
	fi
done

GIT_STATUS=`git status | grep "^#" | grep "\(modified\)\|\(added\)\|\(deleted\)\|\(renamed\)\|\(copied\)\|\(updated\)"`
echo $GIT_STATUS
if [ -z "$GIT_STATUS" ];then
	git checkout $BRANCH
	git checkout ${LOG[1]}
	echo ""
	echo "switching succeed!"
else
	echo "Your current git is not cleaned, please save your change first!"
fi
