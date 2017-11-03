#!/bin/bash

HOSTNAME=$(hostname | sed 's/\([a-zA-Z][a-zA-Z]*\)[0-9]*/\1/g')
PARTITION="pbatch"
MODEL="MNIST"
EPOCHS=5
ITERATIVE=0
OPTIMIZER="adagrad"
FULL_LOG=0
NODECOUNT=2
PROCSPERNODE=2
# Default EXECUTABLE to the one set by test_suite.sh
EXECUTABLE=
function help_message {
    local SCRIPT=$(basename ${0})
    local N=$(tput sgr0)
    local C=$(tput setf 4)
    cat <<EOF
Run test configurations of LBANN.
Primarily intended to check accuracy in integration testing.
Usage: ${SCRIPT} [options]
Options:
${C}--help${N}        Display this help message and exit
    ${C}-p|--partition${N} <val>        Designate partition to run on (default pbatch)
    ${C}-N|--nodes${N} <val>            Designate node count to run on (default 1)
    ${C}-n|--procs_per_node${N} <val>   Designate processes per node (default 2)
    ${C}-m|--model${N} <val>            Designate model to test (default MNIST)
    ${C}-o|--optimizer${N} <val>        Designate optimizer (default adagrad)
    ${C}-exe|--executable${N} <val>     Specify path to executable (defaults to exectuable generated by build_lbann_lc
    ${C}-e|--num_epochs${N} <val>       Designate number of epochs to test (default 5)
    ${C}-i|--iterative${N}              Test iteratively up to the node count, or just the node count (i.e test with 1,2,4,and 8 nodes or just 8, given -N=8. default off)  
EOF
}

while :; do 
key="$1"
case $key in
    -h|--help)
	help_message
	exit 0
	;;
    -p|--partition)
	PARTITION=${2}
	shift
	;;
    -N|--nodes)
	NODECOUNT=${2}
	shift
	;;
    -n|--procs_per_node)
        PROCSPERNODE=${2}
        shift
        ;;
    -m|--model)
	MODEL=${2}
	shift
	;;
    -o|--optimizer)
	OPTIMIZER="${2,,}"
	shift
	;;
    -exe|--executable)
        EXECUTABLE=${2}
        shift
        ;;
    -e|--num_epochs)
	EPOCHS=${2}
	shift
	;;
    -i|--iterative)
	ITERATIVE=1
	;;
    -?*)
	echo "Unknown option (${!})" >&2
	exit 1
	;;
    *)
	break
	esac
shift
done

LBANN_DIR=$(git rev-parse --show-toplevel)
if [  -z "$EXECUTABLE" ]; then
    EXECUTABLE="${LBANN_DIR}/build/${HOSTNAME}.llnl.gov/model_zoo/lbann"
fi

shopt -s nocasematch


if [[ "${MODEL}" =~ "mnist" ]]
then
    MODEL="mnist"
    CMD="--model=${LBANN_DIR}/model_zoo/tests/model_mnist_distributed_io.prototext --reader=${LBANN_DIR}/model_zoo/data_readers/data_reader_mnist.prototext --optimizer=${LBANN_DIR}/model_zoo/optimizers/opt_"${OPTIMIZER}".prototext --num_epochs=${EPOCHS} --procs_per_model=2"
fi

if [[ "${MODEL}" =~ "alexnet" ]]
then
    MODEL="alexnet"
    CMD="--model=${LBANN_DIR}/model_zoo/models/alexnet/model_alexnet.prototext --reader=${LBANN_DIR}/model_zoo/data_readers/data_reader_imagenet.prototext --optimizer=${LBANN_DIR}/model_zoo/optimizers/opt_"${OPTIMIZER}".prototext --num_epochs=${EPOCHS} --procs_per_model=2"
fi


if [[ "${MODEL}" =~ "resnet" ]]
then
    MODEL="resnet"
    CMD="--model=${LBANN_DIR}/model_zoo/models/resnet50/model_resnet50.prototext --reader=${LBANN_DIR}/model_zoo/data_readers/data_reader_imagenet.prototext --optimizer=${LBANN_DIR}/model_zoo/optimizers/opt_"${OPTIMIZER}".prototext --num_epochs=${EPOCHS} --procs_per_model=2"
fi
if [[ ${ITERATIVE} == 1 ]]
 
    then
    NODES=()
    i=1
    TEMP=1
        while [ $TEMP -le $NODECOUNT ]
	do
	    NODES+=("$TEMP")
	    TEMP=$(( ${i} * 2 ))
	    i=$[$i+1]
	done
	echo "Iterating over "${NODES[@]}" nodes with ${PROCSPERNODE} models (Processes) per node"
	for i in "${NODES[@]}"
	do
	    MODEL_NUM=$(( ${i} * ${PROCSPERNODE} ))
	    FILE_NAME="res_${MODEL}_${MODEL_NUM}_${EPOCHS}.txt"
	    echo "Now submitting ${MODEL} running a ${i} node job"
	    salloc -N${i} -p${PARTITION} srun -n${MODEL_NUM} ${EXECUTABLE} ${CMD} | grep "external validation categorical accuracy" | sed 's/^.*accuracy: //g;s/%//g' >> "${FILE_NAME}"
	done    
else
    MODEL_NUM=$(( ${NODECOUNT} * ${PROCSPERNODE} ))
    FILE_NAME="res_${MODEL}_${MODEL_NUM}_${EPOCHS}.txt"
    echo "Now submitting ${MODEL} running a ${NODECOUNT} node job with ${PROCSPERNODE} processes per node"
    salloc -N${NODECOUNT} -p${PARTITION} srun -n${MODEL_NUM} ${EXECUTABLE} ${CMD} | grep "external validation categorical accuracy" | sed 's/^.*accuracy: //g;s/%//g' >> "${FILE_NAME}"

fi
