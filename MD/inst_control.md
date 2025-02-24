

// Describing the algorithm of instruction controller with pseudocode

inst issue control algorithm:

initiate state:
    init reg file
    init insts mem 

main loop:
    N <- MAX_INST_SIZE
    IF_inst[0 : N] <- IM[PC : PC + N]

        consider IF_inst[0 : N]:
            DTCF <- detect data conflict  
            STCF <- detect structural conflict 
            //from IF_inst[0] to from IF_inst[N] 
            for i from 0 to N:
                for j from i + 1 to N:

            while(DTCF || STCF):
                
                try forwarding()
                try reduce_N()
                DTCF <- re detect data conflict
                STCF <- re detect structural conflict
                // solve conflict
            end while

            ID_insts[0:N] <- IF_inst[0 : N]
            EX_insts[0:N] <- decode(ID_insts[0:N])
    
    update reg file 
    PC <- PC + N
    
从已经译码的指令中选择

数据资源冲突
会从1对2的检测
变成n个候补指令和m个执行指令的冲突检测
n^2的规模增长

initiate state:
    init reg file
    init IM
    init InstsIssueQueue

main loop:
    PC <- PC + 1

    InstsIssueQueue.push_back(IM[PC])

    for each instruction in InstsIssueQueue: //from front to end
   
        // 考虑重排序的inst为insti
        insti.ready_data <- detect_data_ready(insti.RS,insti.RT)
        insti.ready_structure <- detect_structure_ready(insti.opcode)
        if (!insti.ready_data || !insti.ready_structure)://如果没有准备好则不用提前发射该指令
            continue 
        end if

        for every instruction before insti:

            // 考虑可能被insti抢占提前运行的inst为instj
            if(!instj.ready_data || !insti.ready_structur)://instj没有准备好操作数或者资源被长时间占用则被insti抢占运行
                InstsIssueQueue.swap(instj,insti)
                continue
            end if
            if(instj.RD == insti.RS or insti.RT):
                break //例如Fadd F12,F2,F6 不能抢占到Fmul F2 ,xx,xx前
            end if
            if(instj.cycle < insti.cycle):
                break //例如Fdiv F12,F2,F6 40个cycle 被Fadd F2 ,xx,xx 2个cycle抢占
            end if

            InstsIssueQueue.swap(instj,insti)
            //insti提前到instj前执行
   
        end for
    end for

    EX.IR <- InstsIssueQueue.front()
    InstsIssueQueue.pop()

    update reg file InstsIssueQueue
    recieve commit from WB

InstsIssueQueue是指令重排序以及后续待发射的指令缓冲队列
空间复杂度主要取决于InstsIssueQueue的大小  O(n)
时间复杂度主要取决两个for循环对指令重排序的操作 O(n^2)
n为InstsIssueQueue中的元素个数