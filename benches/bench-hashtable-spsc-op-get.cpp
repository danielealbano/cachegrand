/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstring>
#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "xalloc.h"

#include "benchmark-program-simple.hpp"

#include "data_structures/hashtable/spsc/hashtable_spsc.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wpointer-arith"

char *global_keys[] = {
        "oemd","jluy","mriw","phqu","bxzm","jaqi","hopc","deqk","rmje","lyvx","mlqq","bfqc","vncj","laxm","mtms","lrvy",
        "goxz","skvl","rtmm","vjno","bwvg","nllk","cgad","wfot","wckw","zlby","hejy","kcud","esny","dbcn","dvcm","ivzy",
        "bcvf","kmbs","dasp","ncbo","ebkz","yvfb","xjvu","qdnd","ikyn","zztf","svkz","nnkf","hywx","thhr","hnkp","yvkl",
        "ctte","fqvl","lymx","gbdw","njdl","wdqb","rdfj","eiky","zdcg","tsww","umgu","qaif","yvjx","tmxj","bgld","abms",
        "krva","qnuz","qumy","jplz","gsao","cqlb","ebaq","renk","budf","mavg","nghe","wzop","kvju","cmha","deaz","xwdu",
        "npvg","mgtu","cpxq","teuy","rjej","omst","elrv","olfy","wvma","pvkb","tnob","jxzg","ttud","udpd","ltgr","odop",
        "soei","towc","cmun","xxrj","jvvr","uolj","rcyn","rlhm","ktdh","krsh","syyp","zmro","kqsm","klec","gtkx","ztus",
        "mwes","dmob","vtqm","jpid","addl","ubxm","wjgk","vafc","zwjv","xxfk","drie","xwrw","zzmn","cjij","paaq","wbgi",
        "mdtn","fnat","uuun","mkzx","btco","gpsi","lses","icza","sqqa","bdes","xtao","vshn","wurp","ywam","axpy","ekke",
        "rmwc","fdzt","rumb","jwyt","ijgv","hzqq","tpki","gjbn","fxqi","vfic","cfrt","uyza","proc","ocje","bddi","vfgu",
        "poaw","wixj","odlh","lxhv","valg","edss","greq","kwez","jmup","vgfc","pocj","dwwe","xmfh","eeke","zlte","ynqv",
        "fanw","tssc","fuzn","dmgb","rkbc","yzqb","pvdu","iypk","aqws","guub","dant","rlfj","jhgx","rosg","vmwz","pfbt",
        "wxms","tfzk","ucfb","ioyi","zsqp","xpcj","pgtz","manv","dvyv","cncx","vjvs","jhvv","djyq","kskr","txxh","vzjy",
        "uzdt","iino","dtcs","kxte","oizl","moqq","fkej","ddoa","tjov","aebb","ovbc","sris","lcqs","swhi","ndlm","spla",
        "tmgm","urmd","lxmo","rpmh","kkyt","llnq","vvdu","egsx","fsma","htvu","yyxj","peru","pfpk","jdmp","fcvd","rhsc",
        "ajqg","maba","hhor","pjwg","yonp","bpdl","utmz","zmpb","hswe","zzgn","yfgr","srwa","qxnk","jgif","mrlv","puvr",
        "ryhs","htyd","hiuh","vuvc","bdzn","fpyg","mqgk","haik","nebx","jywe","jcij","zmlw","hznm","jrfj","krlv","aeaa",
        "mpeh","whzw","pzfb","zllb","pgvt","kspt","argm","zubb","itoj","ryhy","ehvk","kufp","awpa","cnsu","jzzd","zfbd",
        "rbns","jduj","gjeg","grvp","yjmo","rrfc","puad","ovaa","xwgw","cbfp","nzxn","hnxy","mlqb","wexf","ltbi","mcdk",
        "vdkj","tkme","pivb","ggra","qnrg","ggky","gbck","ccdj","rcgr","icjx","vdub","zxfm","lcry","whxk","kxll","bxlk",
        "yiqo","zfqk","ywmc","pyod","vcoy","depw","mmoa","rxlm","wxdn","ocif","glyb","kbty","fqyv","dtcz","ghcf","fydf",
        "ejey","egrq","mpmg","mpom","mgst","xcwz","jxqd","lrzi","hytx","nudh","jtbd","gbqj","lqqx","kdbn","xaul","ytbg",
        "aiep","hjii","ndyp","uskt","tprd","lvzo","rmpo","vejd","bpqu","arbh","qawk","mdxw","amkj","yhax","redc","ncfd",
        "tjuc","ylti","nfpa","jdql","txix","dwnm","fzrb","bfvr","sudc","zsor","etun","diry","xtxn","mwbo","oylw","wpki",
        "eogv","mnrf","lrug","agdv","rtnv","orbz","otgn","iaza","rghl","dfoe","ppqz","cjry","gdtu","wtko","pppg","mltr",
        "uzlt","xqcy","qsll","seme","scvz","ikno","evoa","mfpc","ueku","cuqf","dske","ugbf","onga","zkxm","biuk","swvl",
        "njkv","oxrq","surs","uucf","seug","zjjy","ewbo","msgp","izwz","ktrj","yagb","jstk","wfzi","tqcw","egvs","txqp",
        "jahk","xzrl","jnor","ekno","ralq","njxr","uqhd","vyze","hgwm","kqbn","cmnw","cgkg","uadg","zcqw","rflf","efev",
        "zzvx","gvxk","ufsh","fcsj","tpyj","waee","mvtn","pxkz","kqms","anom","egxv","kiyq","oovy","gare","uafm","knav",
        "tsnb","ccuq","mcsc","mkzv","qodw","nici","rrgx","glzz","ntko","aimx","kbqg","dxeu","aabh","mfsw","vdbz","tidh",
        "pkmo","hhkr","oobg","clkh","kzfj","dgqj","kdes","thia","ymue","rplf","mweh","ehkw","wyjt","qpmb","snmv","zbkm",
        "zwyc","mqii","rwhm","buzi","afdn","uzek","ftlz","pkvi","mjwn","cikp","mwea","cday","hwur","axpw","gmrv","nxom",
        "ppwp","fabw","kvbv","rntf","lxaz","iiab","hxix","iqjv","fssw","hzty","mgwp","rckr","fler","cmal","tuuo","iiux",
        "jlhd","euzm","zqwl","weai","pylq","tctt","cdrj","vrkg","ppni","dfjj","jwgw","itwj","nmcf","kfpf","gcnw","dfpp",
        "bueh","nlhn","isys","skko","lqrf","lieu","ijgp","eeqg","cnym","wkev","kqly","papp","aifu","ctve","bnlp","polx",
        "ktim","ftcd","puse","xsrm","rmvi","hify","peyq","tnhq","ugvo","anpy","tbch","igul","ztxe","kaop","alwy","kiyx",
        "bpaf","qahf","nfck","qirn","qbzq","kzeg","objq","cjzi","jzfa","pztu","ftlo","rose","zprv","rvio","jgks","dvic",
        "vlli","fgqx","vrxb","ngif","bueu","ceiy","hnxj","jnug","oqvo","muqs","ryhx","ujke","jipn","jujb","zxye","furs",
        "wpmh","ehlx","nlfx","eapi","scgx","qaup","uwpb","hlqb","fqgy","nlso","mcrn","jiyh","tigp","ijgs","eadj","jcsm",
        "mdsa","skku","gnnh","uyau","pims","ybxo","sptc","jxrw","fqxf","ecvm","plxf","lucb","neak","cecr","wvkv","mntk",
        "gysu","fnsr","ikpo","zegm","jxer","znqp","mhts","oykt","elih","jaad","ihec","bthu","mmor","cekh","okmu","wrac",
        "lfxr","zeus","kced","svnc","qfox","wxvl","uqrt","msib","nmox","qrcj","avyc","elya","urul","qkci","wioi","zxzr",
        "urtw","tccb","oqja","ejlr","iahc","ingv","yjju","ehvl","plty","coie","yzta","itad","rxkx","rxnk","lrgq","qlyh",
        "sgfq","ebyq","akad","jjog","ssee","bszh","wkcx","lgap","nxkx","anzk","fzrh","bgdk","srzo","xfqc","mjoy","rusr",
        "bcfs","xywe","ncnu","fyyv","mfze","fhba","shht","xros","rfxr","gpxk","tgjh","vgmo","rllu","gogb","gdbd","bhob",
        "szxd","yamh","aeyy","jxkd","wnmw","nxhm","uolt","ymph","zdms","vdwe","jvmu","hshc","xoze","dkvx","ewsc","wmjl",
        "nckk","itzb","zunq","orjg","zidy","wvas","cyij","khra","clqo","erwb","mamb","tsch","zuir","icrb","wwwu","xkod",
        "xeeo","ugfu","fihz","osxd","baij","jcoo","zogt","orib","gmnd","wwmg","eilb","bcll","blbt","ucjy","tnto","dfdz",
        "rrtp","kjdj","ynxu","aswn","aztn","gvdp","jcsf","aceh","yalp","ayga","znjd","thes","sxmk","sqze","vlbt","ztny",
        "rfby","dlob","wxqy","hdnm","jlxz","zmvo","wizg","rbfv","vvyu","sdzi","uaiw","cslo","tycf","rlrs","wiis","lkeh",
        "pmtq","rkoi","ewbd","uokr","dist","hqjn","uqce","zafy","smdu","aebz","rcuo","msdh","krtu","xkjt","dgxj","eova",
        "qqct","iefq","ylys","bwrl","hjmj","tgmy","fegx","eqvz","ldil","jypr","kzgw","ndzh","vsig","xpub","jfcb","bkmb",
        "eerj","qoih","jptr","hhih","vxie","qbbm","taro","pqwc","gkft","vzam","ytxb","wanh","skiy","shld","ihog","jzdb",
        "igkq","bjvp","fqvt","hufv","qarc","kvnj","qykk","jcrc","aocd","uuqr","huve","rosu","ejdo","knku","oreb","qlnr",
        "krmk","dpid","eevy","ibhx","dceq","rqrr","vgcw","azfg","xhxh","patu","bzcu","xslu","wsoc","pzhg","jbla","yxih",
        "ptyw","egfl","syrw","fbhz","hsyy","tabv","vpqi","lphw","iydc","swdw","fbrs","eovg","eaqu","uftg","kryd","lsnw",
        "ddji","ulxe","ajsu","zixr","rqwc","cavl","anzt","coki","qcis","hliq","txoq","jysr","cfum","tquw","imcv","zijo",
        "zooe","nlzi","wvwu","unfz","dzcf","sofe","hmmd","hdef","lbbc","zmox","kvmi","qzkf","raxm","dbiv","mbwm","yyod",
        "eyph","fbto","egfs","mfcc","caww","gooq","pqlo","thjk","umjo","htga","ygtx","pqib","lpvw","tlli","xhhj","glfn",
        "jwme","soro","cquc","jpxp","hgkh","ftol","ohmr","hggd","evhs","fmlv","dtiw","wgpx","pnqe","kmrq","xymm","gfeb",
        "oonx","hjei","xugd","bzcw","jpck","qarp","zead","zxbx","okeo","fvxk","rgni","wufi","zjbe","hsby","riyl","rukc",
        "tjal","zias","otwz","mkdj","hdpg","utpb","hhoz","olum","cvji","jtfp","tsua","dunx","voqy","roho","dgen","mtod",
        "kwpo","sidd","hkfk","ugjj","file","krnn","lgxe","xbjk","vwbr","xngh","aciq","klyi","kcpe","hcby","iucu","cbep",
        "orbr","iqbk","ktyl","iuzd","wuzo","uptg","bzoa","yvzu","ybdv","bvir","kmwc","evuq","ihnk","pkyn","arqd","hllf",
        "gjpi","uxxo","qoni","epoc","ypzx","tmhi","umxn","swcx","elqe","utrk","blkv","htrw","tqai","foje","clna","wckj",
        "avrh","lcct","ckpn","hpry","subk","lpzp","wdzy","cgxa","fqps","lwol","anvc","fsvn","oqyy","cfhy","tqmv","ntbb",
        "dmqy","wjmh","qmsq","dxfr","ojuu","xgwj","ygmc","zrqv","bzwj","xvvu","cpdf","wavs","crkq","kjmj","lxpz","eayw",
        "hscj","yaen","pvkl","qumo","nnuw","mfdm","vknm","monj","vdjb","hitv","llgf","tmte","npio","jpcs","qacb","iuge",
        "gzgt","awui","ckxy","bnhj","kruy","ylwi","ylnm","cczc","ifih","zyxt","lftj","dwhc","osxv","tffo","hxzr","ggxx",
        "jpwn","nfhg","rpra","mvvu","pzxf","xias","xlme","tuvr","ucav","sdbj","twks","lthw","fkpd","lljq","xekl","zgre",
        "uqnw","lxgo","xtwo","fbrd","bksr","idvy","mozy","ynfp","lxkk","ylym","gffy","qdfb","yhmm","ffof","fzzj","ifry",
        "ktkr","pbat","bhqa","zmqc","iqjz","ffpe","ezxw","yirm","hsak","fwfg","rplh","dfmh","ajti","gpkw","iveh","hlcy",
        "ymbr","vsiy","eqns","ludj","qrfn","vwvq","ozyp","xedq","cvpl","wnoi","ixqy","yjpn","gtvh","vmaa","ausi","vkui",
        "mtgt","zbeh","oybb","pcvf","gpms","ntyf","kdry","datm","cihh","njtk","apgk","xyoq","aibs","cgzk","qovd","hjdx",
        "hvwy","vgto","eauz","nawq","kjzn","bayg","xmzx","vztz","fbie","nnmi","pwus","ktjz","zvdw","lrmq","uqqx","qthl",
        "mxki","xirf","ytck","skhg","jzkl","ejnw","ifdp","eohv","klxa","dwqp","gdhk","mfod","afaz","hjjy","fbtx","mmrl",
        "mcgl","tilp","qsjn","wnfv","tijk","rrmj","byei","mfqp","eixo","kqxx","jzja","hnmg","oiev","ijtk","osqq","stfy",
        "dnib","iwsp","xvtk","mdrp","cxta","jhqn","wrjw","kdib","flql","gkia","hmln","zxce","wytl","shiv","ugou","vhtn",
        "rgja","riqq","gxgg","fsfq","uyrq","ecco","yehc","flll","yycv","jxvt","zhni","rzqk","absd","fpop","dubv","ctqn",
        "terv","hrgl","updu","zfsl","xuym","cour","acua","fnhp","dzix","ofqt","bpax","ityp","eqpv","cebw","ndoa","bclk",
        "eeie","frry","paex","vglz","sfzf","iwus","nivz","pcfo","dftm","eils","udll","tyih","quku","adkj","jcsx","vnjf",
        "oqot","pnfb","jatg","wbqc","sisn","dign","pbdy","wfrw","dldb","ngsb","ivzp","umsc","buqf","fzfh","pwef","qief",
        "dvft","mbpx","qeln","punk","kmcw","fmxr","kaqa","ocra","lmub","vqam","ouyu","utrr","zpre","saay","kryo","room",
        "xjoh","hhwq","kagt","qhkc","dnxe","zoal","pwal","ccbg","kcze","ujcm","blwq","rfke","etou","olnm","kvqw","iyzt",
        "avuz","cwsq","isbr","ibwu","gkat","oxif","uobc","lrqm","uzur","hqxo","dfuw","bimp","imrb","tdcq","ednj","ktbn",
        "yzeu","iaay","bfko","dvlf","atso","jthx","olyq","rpnp","dhke","iaqu","pkdh","jrkz","neyo","skdf","cylk","zjyu",
        "juzu","svhn","scmy","jvvz","pdby","kecz","bhxv","ksjz","dtjp","hdvb","ewhf","ezbh","kyak","csxn","nfxj","ymwq",
        "bkja","fjvw","masy","iqod","zmbf","jrsd","ugri","smaz","lbvd","fjrp","pdir","ngos","qyek","hyoq","vdkw","rtad",
        "skau","cstw","rgkw","ywmq","zuye","iahe","faev","dayd","isvt","uyag","hpun","sldi","oakd","orgy","spij","tici",
        "pylx","ffaf","bjsz","wezk","ciur","clzx","ajjw","lekj","wseq","ramw","yogk","owmy","wdsc","wsnw","kprw","fwar",
        "ekjt","lqyf","wold","gvvx","qoiz","gstf","sakk","whmm","odrk","edeu","vyjk","wnyg","ovau","xzaw","reqw","hygn",
        "iteu","jfsj","eluz","brwi","iwzn","dneh","byjk","fnhy","mpar","fsbr","lyjl","yxmx","zhng","xneh","ckna","ucuy",
        "gsrm","krwp","mxfz","buza","rocv","qqlm","gxwc","sevz","azpx","fyzg","ouwg","rxqs","txae","ubbf","osia","zrdk",
        "erew","sdiu","jbeq","rmfb","rlgz","pamm","ainv","jfeu","wyxt","bknw","ttnp","xebm","untc","rbpx","hjts","ovev",
        "wkcn","lgno","qlmp","wiuv","oxll","mkqj","hxqf","iffk","dlnq","lqva","enef","njmj","rrhi","gvna","aitb","iudf",
        "fmdl","jdey","ojyq","ygyz","rlgc","lura","ashh","cewj","sccz","miwr","mwnn","wgjx","syft","vadj","cgrr","kxjg",
        "xogw","mzph","eioy","wzim","srfx","xnjd","nqtr","xfox","dege","fhic","gwku","vwwp","king","lxqq","aluo","hqmb",
        "avpe","ggjj","tvug","cbna","hpkf","irqy","kaqm","rcqi","vmme","xwcq","joya","djpg","ajba","hpsp","prkk","soxw",
        "lyze","pjcs","rjmu","bofg","othd","qynp","aqeo","alyl","uybe","mjsn","zwpo","gvpw","zzqp","qxet","fvgw","lcnu",
        "cekt","xknz","wfox","ulhx","yrdp","ozuj","bvjg","zoup","wajy","cqms","ujxn","sdam","ijwe","fjbl","fmnq","jheu",
        "ukas","wdgg","rwzi","pcob","vlig","wdcc","gwnq","apej","hlso","agym","fexf","viww","fzjh","ukky","jerk","hwbh",
        "tpzr","uwaw","eznu","ryod","tzhk","kcne","uepv","owxy","rgxc","clfh","dkrb","yloh","pwhq","ceuo","hnpy","qlnj",
        "kkaz","dyps","yfjf","skqb","chgw","ddvg","sjpx","xuhp","gimq","bdaf","kuel","efar","wrtr","gigx","chfs","zzkv",
        "jrhz","thjx","ujvl","smbe","nese","sxli","iknp","evkb","mrlp","urob","wcoz","hfmu","tilx","bhhj","zwyy","wotu",
        "pntt","ozow","ynpb","jrsm","pcxq","cgqd","cqru","lwgw","dhjo","ubze","zvsj","zkoc","qktb","xvbt","fbms","yoxc",
        "llkm","bsef","anjf","lfcy","jdhq","hvka","ubzb","fwyt","vfhm","apyh","ytng","ysyk","npqi","ofgr","cnac","tuze",
        "vcrf","ayla","hkrq","lyej","seww","eqvn","wtab","ywmw","ukgk","gmqm","ople","pmdn","vzop","cxhr","wpta","kycz",
        "lrat","flku","xlld","ztxr","ehyn","mgoc","keed","dcgf","twjm","gptz","pzrc","jcet","othq","uhyh","uqro","fkql",
        "tujs","amjb","mxif","arrs","abiu","yuud","vdwi","ldnp","pczb","yxjn","xyzp","kiac","tslv","elza","lgcp","vbaz",
        "woli","lmhw","jkyy","pqqu","dblv","doem","hffd","agem","zmah","obpn","lleh","ecuk","reah","twnq","qlzg","bvcc",
        "zrqa","vxqs","ojea","hvjq","aihk","cann","cugx","syar","izib","hmlr","wrsz","yewl","rbur","qmms","jbqi","ovpm",
        "hulk","omyy","kfzx","zzgp","hkha","jqod","fylu","abzg","jjzg","obft","owug","yefs","avhr","ieuu","favr","sclk",
        "ufvd","mkaw","rtoz","qzle","cvah","ltbh","asyq","gszi","dfry","uioi","soxa","ozjq","acga","smef","kgzy","wzrh",
        "eibw","pblp","qthp","yxuf","qmqk","zmjq","gwvh","uedj","rkxk","pvaj","hrhz","tnha","cgxe","tfyy","lbuq","jycl",
        "anca","uehf","qvnj","etwq","xfid","gqlg","rooz","gbgi","cppp","jpcp","gfdy","tpld","ragl","acmn","rndj","blkn",
        "bcux","ndud","gbho","blug","bpub","takl","qozb","zlbz","vfbv","iiyn","swbe","kocp","yeyo","fatp","kzup","bdxk",
        "wyog","wxkh","ixez","ghpj","vstx","otio","yqfz","zxdd","fvbs","uiok","hnka","orav","uzbw","mbhf","lcam","jrkj",
        "aobd","izum","hixs","qolb","bewj","qxjq","bxtu","mcip","zfxu","oghs","opza","mqtu","xtpo","etva","pmkm","vzfc",
        "icdm","dkap","nyuk","wpfc","payu","eowd","tpen","lveg","kuxi","zahi","kzqf","tegy","cfmw","owqh","dueh","hddn",
        "xjdg","appy","zmat","ufzi","swhn","btqb","jqcs","fjid","yylk","zhcf","rmvl","jsgb","qmjc","tfwx","rnaf","mvgf",
        "amyp","nxbp","cium","ollm","dcyq","hict","zdfp","srmz","vyla","eiso","dcbp","mrcm","peji","fysw","reoo","gcoi",
        "dapd","xzkw","ppvy","petd","tbpz","lttc","owup","mfbz","winh","xige","soju","yjzd","gjph","appk","leth","etsi",
        "nlot","ppcb","nuze","soqr","uqoa","pspz","rfrp","tmqw","wkwh","uewr","otrk","ymca","epaf","qpwz","hmjz","abbj",
        "vehp","ndeb","edik","ogah","nmuu","pmmz","hcgy","xmtg","ygkn","akmu","edux","bemp","fmrb","fyjj","tzoo","kwcz",
        "ssfc","zrqp","jgys","zvkz","zbmd","miss","lyqq","orkm","oawv","abgm","xujs","tnbe","qbgt","bdmn","fvib","uiai",
        "cthk","zrfo","toia","ayko","ejae","lofb","dwmk","hpvs","egql","lumn","kxwf","ayvb","nwhb","pfrk","yand","zuyf",
        "fkdd","lksd","gwxb","sdcd","wzrd","zdhw","awen","kvxh","cvsq","tyvl","omhk","kqyh","phlu","ppew","uurs","zhkr",
        "iycu","htlq","xxpe","uasu","komp","xbki","duev","uyhi","ntcq","tfgr","gnwj","vbwm","xhsr","yata","qzyx","mdmq",
        "kbuy","pvmm","lvbh","pnlq","nuar","nwsg","skxf","dfid","rvjw","nukc","tmhd","kykk","epdi","ykpv","jpkr","jbyf",
        "bifz","ukex","jcsj","vfvp","usis","ytlh","insk","lrax","iykv","guqk","tjmx","adcr","vxlq","amsn","zctm","rbzr",
        "dslq","rihj","lqil","yler","dftv","zrsx","udxv","qtbk","lzyh","lagr","kfin","itrm","huke","yudp","zrdn","wtmk",
        "lgfc","mtsp","zgbm","yjut","ryqe","xgan","zpwd","pshd","xmzw","webv","wydg","zkma","pmso","btqj","ezjr","unas",
        "yqkj","phls","lxdh","gzsu","fgvy","vyqs","jsux","tiit","laij","nkwd","dhgi","cmtm","ukrt","ftte","osoa","sflc",
        "kyyg","hlid","bbdl","brwh","vkak","htqw","wfoy","arab","rebw","dgki","mfai","xfer","exob","dmyj","suvx","usiw",
        "iper","fzpg","xtib","vaqo","pgnw","pece","gbzd","inpj","sgpy","jhjn","vfmy","grxd","uqna","qhfe","pnrs","wwtn",
        "byub","nmcj","retp","rmjm","wwlz","hlrm","itfv","tync","kzki","wgtw","sptf","motk","gdpa","aldo","jobt","uxqw",
        "sxkf","wtnt","mdde","sfqd","gusb","dzmq","ngrc","iusg","kmfk","qmjn","xnmm","imtg","hqqf","gwlg","vgrl","bduv",
        "ybgd","prsv","hamq","hjrt","dove","uffl","qeks","ulpp","hnfm","oxjx","kljd","dnee","stgx","avhc","knom","ddho",
        "jhlx","wecr","bmlx","ozdq","xtki","ipnk","ivsb","jjdr","yljp","qxst","rsgz","pbdu","eopu","oaxs","lnxe","syyh",
        "pukc","umrh","zkuj","admk","ocdm","gntx","mnlf","eivu","zgyc","kyoj","rsjd","njne","ermf","nzma","plvt","zoml",
        "fjen","sliu","sdid","xdub","javj","cwqt","mzwb","gvjc","coza","xcpc","eivy","qzve","wbbi","btrl","ttap","kfvg",
        "fmev","njvt","trki","mfgp","kgmj","reul","mabk","gmbh","nppq","suoh","yxzs","syws","xrlr","nhry","mwye","gukm",
        "zazm","lxvl","ixzb","texm","gypu","luqv","heun","gmuf","aenk","ituu","vhev","wztk","rmcr","dogm","fgzq","hbbp",
        "cqlr","bokw","wlfo","snrb","ysaw","bbej","xacm","gkmy","vfuq","hoga","sacb","uvml","xigg","bfyw","xigf","oqan",
        "olpy","jfhs","bcvc","otrf","dnmx","ekvt","onlr","zfei","bqze","ckoz","wava","nalz","alas","nyre","dndm","uzwy",
        "tuab","qqkf","yuaj","yoje","skzo","febt","mrbt","uxox","flyi","vzyv","kslu","awfd","uxag","xmjx","uibt","hpvf",
        "pssh","vpzc","ashn","qojz","gmbw","xtyp","mmup","appl","luab","zjhg","ijai","tbyf","upvx","ibfw","ynxp","geno",
        "oyiq","gzkv","iyne","bkog","oimi","uoxg","askc","ocyb","agkr","kdcl","rzpn","ajgc","doaa","dvul","nxzo","waer",
        "tsta","crfo","birp","hlmk","kejk","zcmj","lfpy","aubx","gjgc","mdyu","psmc","tunz","dirf","oshv","tjft","qiln",
        "vhnl","yvky","gycu","tkbx","hufk","qwob","qlxh","wwbf","zzij","aklj","pmoi","cees","qfjk","zipy","hzzx","nrre",
        "fxgf","nbta","gova","rlnt","xect","vbgs","bqex","tlrz","oalh","sjkr","ljfx","fove","mppt","ankv","wlto","qvkb",
        "ymvl","zuii","pmzd","efoy","ceri","epsq","laeg","mdjt","ohde","ywvx","hoxa","whmc","vhfc","wsxt","bdez","jwba",
        "yttf","dsai","gmjj","bkdn","brih","qyum","ljbh","jqsp","fmaa","axcw","uivx","ehxf","ouyv","zkkb","dmik","dhst",
        "mede","hfkr","ihuj","scoh","axpe","ufvy","sfnr","aaaj","pwcw","mzhw","fcot","ailp","ddym","egtr","rlkj","ovel",
        "ahhf","hjnx","azri","kjox","aavt","xeqh","djya","vzmy","lswh","nmun","bxzc","mwbi","mipy","kcpg","cbut","eldf",
        "ylub","lunk","xkue","xuen","sffp","euau","hgoz","qotv","coct","htof","ecsa","bvee","wmyr","ruhl","wrpc","pazy",
        "atew","bhff","izvw","ogfy","pfsg","gikl","iyhm","uhhc","glvk","gapt","biio","cnyy","ilbq","vkzq","voke","hdbo",
        "jcwk","cmpu","seem","ailm","eewd","tagh","hebi","pqbn","hrew","dbtx","iupz","ycoj","lave","uwnd","muwy","vwxe",
        "lteg","mklz","apel","cwty","btdj","pnoc","gnzj","vvir","rekn","kvfy","tkwj","ldje","yydx","xubu","tszv","jkpb",
        "xngx","nnat","cdwr","jigk","obkv","aibt","cmco","ajaw","jhpv","eevv","rgpz","kgqn","ktsf","sdqx","hgmq","utvu",
        "tuxz","ayyd","vubo","cihl","mkqi","zlgm","hqlq","ilvt","hjgr","bgmu","mlga","ugim","rgwb","joma","xoig","evze",
        "ukvq","mcou","ugna","lbok","ldzh","atgx","ewyb","htnc","iwwc","bfea","fptk","sxtn","krmi","xglf","rmyj","hhfk",
        "jpot","zfyw","atik","cwaw","eqnt","fzgm","tdmz","bolw","trdk","kehv","cjyo","anyl","qkxb","lice","ejxf","ewma",
        "gxcf","miqb","ojay","ircg","tpvp","dgbi","vqzk","rlwd","qjas","xgck","bmus","kvka","oria","bxkl","fswp","xbzb",
        "ynlx","ttba","ctkr","dnld","rfgz","qufd","aora","bhnn","rcby","axpx","jwam","ocuh","vtqk","ryqp","cuex","xvlz",
        "dkbf","uoeh","cuiy","quva","sbvu","hrrd","ptbw","pqtx","abqe","azrv","ljup","hfmn","hkib","rtxw","ajxf","llwv",
        "jmxr","pjow","wfth","gkbh","coae","kbsk","zara","sdap","oigi","cybg","saej","mybo","qmer","wmlb","acyo","lqsn",
        "zlpu","ccun","wqgg","bfxr","qzpg","iqnd","pkqz","iwmg","iizf","bjki","xgic","civp","dure","sywi","onuv","ozfy",
        "lotu","sucg","fhpu","fhmr","aazn","skkr","stwy","vmjr","ilym","wqql","jjer","sggx","imwe","ixsg","udfs","pwbj",
        "usyl","jmnb","yehm","wneg","lkgz","lfac","anai","dwdg","ojrw","vkuq","vdse","yjqk","pije","uhmg","bwbc","svzw",
        "vqae","gsjp","abvz","aqtt","zald","qoey","rcno","lkcw","doyv","ablc","mlbw","dxzv","vlql","ixrn","nawn","kpug",
        "tupn","rwde","uycp","zsbs","dhec","cnth","fqrc","bqst","yclg","mafh","rqqd","juwh","fimw","sbja","pzjo","fefs",
        "zvgw","pzeo","yayl","wdko","ginn","ygrz","nppm","gjnu","fety","iwot","pdbk","xmxc","sqjh","ohqi","xkzq","tnnb",
        "gizs","mdnl","fdrd","selp","ezhy","uhxa","tnfk","sptj","ywer","qdms","xogh","swby","vwli","kjxo","czwr","auyu",
        "sfpx","pvse","nktu","usgf","ypgw","fdjb","yguk","bkxh","frwj","cyvm","xmgv","xndf","wpwd","jpmv","iexg","pxtb",
        "xtyj","zfsh","wlox","xfon","jtpe","jkwr","egku","adxt","zieb","lwmr","ijxe","qsnd","xocf","bqaj","tufx","ixrr",
        "yexb","hyao","rmqb","kgvy","vlrt","gqqj","ajjc","enhc","aqpu","duoo","adrx","ryfn","zqwj","hiln","vyfy","wnpu",
        "gfqj","pzlh","klzc","viyd","arfm","htuy","pjyz","rzce","xkal","ktsr","fari","gyvc","xifu","hxna","pdqt","lrgw",
        "ldcr","qcyu","ippc","qmme","myyd","hegg","efkz","rutq","uzlx","kfus","plea","pcbo","gzjz","bcnm","zjei","jcob",
        "vgbw","fanc","iyix","smgd","ljmk","sbgn","jcfq","skjc","qjgb","cytl","uhqs","kqmt","bnqf","iawi","fncb","sdnt",
        "soji","uctx","eqer","idhm","lqua","nfvd","nwdh","igjb","xamb","lgrb","lmju","hzlp","nmpv","myza","xxwd","hbyl",
        "lczu","ddua","eovd","gtbp","mldr","wbnv","cnho","xcai","rreu","fbdz","ixrl","hfyx","xoxa","dvcd","bxcp","xfia",
        "fvjt","duxs","yyar","iagh","eadf","kkbw","uesl","hfca","jkwb","dseg","ubec","jmux","ceqb","vdqa","psnb","zykh",
        "iozg","nlcc","ksva","bptt","hcxk","azal","aqbt","fymr","pvfo","bxlm","jblc","hchn","ftwt","ppny","frrv","zuxd",
        "wdsk","falw","redk","kwij","puri","vwfe","hbjb","krpm","ujiu","cdwi","pddm","szkr","well","ojty","cnpk","pizj",
        "xnsl","asev","euly","fzrv","lysr","xxjt","pifp","azto","okcg","oels","ffwj","tsre","btwb","nuaf","fnxv","nzdc",
        "wwbm","krnl","wwfd","hxun","fjgv","ppgx","nwfl","tgnv","ancp","nvnf","pelh","dbjd","snlu","jcgq","gtzj","ilnh",
        "gknx","dici","oogz","zomt","jreq","tzpa","mhhu","qfsi","zopc","xqhx","mota","yjnj","wxmb","aoxk","pzco","ppox",
        "yrno","xwae","soct","umbc","djsz","bino","qlek","grhl","knzq","ircj","hchw","zjem","hjof","rmip","xctx","cvpn",
        "oirm","ifsk","abgc","huqc","rcmz","mgek","yfqi","bpiw","iesq","jxty","ftke","izve","hhby","hzlk","inwy","qzfb",
        "tkmx","zvts","heio","fayn","kmqv","mlor","vhtc","nhpd","haau","qbhl","jbnn","olgb","unfj","fllc","ovoe","ynbf",
        "ljcw","dxna","prvm","cojn","mepj","utbv","funa","yyyr","ccmx","xkdn","bwjp","coqn","ubvs","pdan","jkba","axts",
        "rxsn","quck","tgkf","cfrs","uaxs","rmjv","lsev","sywy","jmym","uoml","wasd","tuzc","chgr","wdva","flfi","xfjk",
        "zhdb","tnlo","byng","kitq","kjwu","hawe","rdzh","tspn","qlwl","vhta","dcdo","wbpn","wxpd","gzga","ijal","etco",
        "ekaw","hkwx","btmd","kvxe","cxqu","ecpv","dfyx","evrc","klrs","mxql","ekfm","vpbx","cqte","phan","dqnw","pcft",
        "pbcf","qqpf","xdce","dspe","vnhy","gjvz","unxt","tect","gotg","jojy","xwud","awjk","dexs","dduf","eumy","grop",
        "xunt","pjnp","tkps","ijjs","fhjm","oboo","alqy","vuwv","xbue","jdad","tnsw","nflp","xccq","xsqe","rwke","zcrr",
        "mwuj","gidt","kpoa","yuog","hdxi","abip","lphg","rjzo","gtbg","flhb","rgjy","rljh","whyy","xxxv","gemg","iqso",
        "goxb","iqjo","jnhv","oebv","tvgc","rrtu","rjxc","nuet","htpc","lhuk","bwll","ghdf","ttgs","pplg","hhmx","riry",
        "gtjs","wthz","doho","tqad","nenp","kevf","njda","lwkz","ojyn","awih","banq","tsmf","vhtf","tlrp","xhcz","kubi",
        "gcyw","mkmy","quvk","vxli","oicx","jzlz","lzpb","hxzi","puna","gekm","aziq","pugf","wfjy","unjg","xwbd","nguv",
        "yemz","kfum","wina","jraa","ipoc","dbxp","nona","gekw","ltmk","ihzu","wsuw","qbas","tfki","dkbz","nyeg","lwqw",
        "xvrx","rwqs","rorp","uems","ywnv","kfqy","xpcq","twme","ehmm","rwrb","zueb","okdl","xfbl","xvsu","nbxs","eoqp",
        "zxqx","kcoy","rosp","yhhv","piov","nhoe","oday","wlyx","aqng","hlxq","tcki","gaxc","hzdu","ofrq","qbpk","itnx",
        "suqw","tiap","fxob","xrrv","hwkn","crxc","ockx","erob","bdor","vixm","keea","vhes","rlqs","ushn","kslf","wppi",
        "nifb","amyr","maco","wwlw","tvab","rlvm","uyxv","wvxh","zpzk","suxn","lwim","whmq","alga","ygok","rrcs","tohx",
        "bapz","aabn","ptdn","ekqy","iwjw","fywc","vdzc","xpdr","lvsm","olnh","mxov","gkie","ihkb","nzai","rcqj","lncj",
        "uydy","yylv","cgqx","zpbx","njjy","qxop","piti","aarj","rplx","ofhl","ltso","ihky","uivz","eiki","gqcq","xvqf",
        "nygy","htry","mtje","pmzw","iwgk","ifhp","bvqz","fquo","likt","mmxu","futl","dhgv","fazy","gllp","tkcq","hobg",
        "xcah","qfrt","zekg","xiyq","vuvj","umsp","rxaw","fsto","iimm","fjmw","snch","vunz","akwc","nsrd","jxys","wyuw",
        "nboc","icqd","iujt","uolg","ifzl","fbzy","ovqd","sadx","ggrg","adbu","bwwg","kioi","wpea","uqrs","uynh","qoqv",
        "zvev","ffhn","rnok","kugj","fzhv","jrng","onbz","vxpn","ekzi","oniw","qgxo","pnhy","ugfg","jbgv","pfyk","rdxx",
        "pqqc","pjpp","tzef","oeto","imdv","yiif","qero","jmgz","clta","haje","bbhu","oros","nvkn","ecmy","coiv","tdml",
        "cvou","tzre","bbeo","ksdq","lbxn","yofc","pkvp","jklf","gudf","lkbe","mzjq","dkpu","nxgi","rgbz","cqtu","qejf",
        "gvgw","qjvv","hxnh","ijpq","rupx","yoxp","vqih","ayhy","scyb","qwwu","twvz","fzhk","ndcl","okao","frpu","kbzc",
        "qyju","sivt","hrfi","dwtg","livd","hnxf","fedd","ohpy","phbo","mcgg","qqkl","bnok","noye","uqxo","cowh","wlqx",
        "rgnn","zwda","bzip","wsxf","yoyw","iqrz","pgga","kzdt","rbdy","ickr","kaca","koun","zqwa","iuns","xjef","qvvw",
        "cwul","kemx","stpc","mwpa","sjhy","kzgb","sjjr","qlzd","akhk","pnjz","whkr","hdcu","csir","figt","frez","rgoy",
        "ichu","ninr","vthh","ifhw","spoa","kqsr","zoed","jemy","pdzy","dbre","trvt","nfui","gbkt","omoz","hgcz","doql",
        "wnwk","myhx","spgx","ojon","qdnf","iatq","gemw","wrbq","xckk","dvvv","sksh","eqdl","agit","eocm","dvqy","irys",
        "seix","mghz","jmsp","uhpq","uyqi","hxid","yycw","bkox","mnhy","wgvh","svmz","uicj","lyyg","zvlw","qivz","anbf",
        "njvg","zugu","ozey","nycu","rill","tdfb","ahtf","ahhb","uikq","hbcq","tzns","oqrr","bhcj","teuw","riid","qgcu",
        "gixd","gnsf","kjnt","ozul","ojnt","trqz","hkal","qjio","gyau","dlus","fvfi","kmgk","vfjc","ogaz","aezv","xovm",
        "rhey","osoi","jftr","neoo","hyaa","yaos","scah","qyqs","kqnf","mjhb","ktnk","gqbb","bfqk","xrcn","yfqc","einv",
        "asax","dsda","iswr","nsdw","gcxw","srer","wewz","nvtk","qwbl","bxli","rwph","fzug","yynq","kahd","fmfz","xoiw",
        "ulrv","zrqj","jdra","iipq","mfvp","rgwt","xycd","tfqm","wxak","ecmd","tobc","byin","amzl","yrjn","uraw","dhzk",
        "yvyj","zikb","ijih","nddl","mvyx","lkmg","sqqv","aphh","otjw","aftp","nnde","lmxl","gfnp","ravi","vank","prun",
        "wtak","vrfd","yylr","wcht","ykul","mgnt","bjaf","tgqg","cxyl","iapp","xkjg","uzqy","iltk","qtoj","nrvj","hqqz",
        "zbwy","lean","cqis","rgfv","ggih","jghs","enzu","jujw","uizi","gkhg","ehqz","oybp","kkfb","tmwk","xkdi","ulda",
        "geuk","hvtt","rycv","iysj","imbh","pjzj","ledn","vdml","vinq","qyaf","ltwo","mzcf","hcrp","phqp","jpfv","gvkf",
        "qnnj","wydc","wfbu","sxsm","njuv","ldte","momm","azkb","shtj","fxyg","eqak","sypx","psyb","ibuk","vjei","xvzf",
        "iykm","lbah","egtz","xxrl","yskn","yjys","nlli","uufx","cpts","uzxv","cxkf","yxfo","edzq","amgw","gpxc","ehtv",
        "vkpo","ufzu","rejv","vdrc","uxbc","xqje","doqx","psxx","mnaz","gvqd","bthh","pxxy","goyu","eroa","mgxx","vvni",
        "dpan","opvu","ugdw","wfmh","hdpm","sunx","ycyw","ayle","luxs","kdwj","bpky","qfwi","rlix","akcw","iwmb","ozug",
        "rani","wimc","fssl","agjz","uunm","nkwa","zwpi","srlh","derd","hntd","veyz","sgaq","zode","wgbd","adyu","xszs",
        "gfun","qhts","xxcm","dbrr","szoq","ggxe","bkkl","uzzk","lvws","vjun","tvwo","qhpw","gqav","lopt","aobn","gxxe",
        "gpwp","iqvd","coff","awwv","fpgv","zswg","bgsa","xqdx","xwrx","ooub","inqo","junv","jexj","vxqp","ztdb","drfg",
        "cmds","qymt","ebbk","vwji","hafy","xfph","cqgf","zkrf","nkus","dtej","gdgu","oqhl","ywff","jfyv","lvix","ynlo",
        "ontl","ezzb","wqjq","pvjl","picn","yccj","rltc","jpgj","xrkh","zzxl","hrja","gsqm","jqao","nrpt","eykn","xusu",
        "mkkt","wcqy","zunj","ofai","kmmn","gqwm","vdwq","tots","jhid","wrhw","qdsk","eigo","gooh","rxdb","csza","epqp",
        "npnk","tfoi","avho","mhcx","pndb","kgmu","ynoa","mggl","jyom","vgtz","ubbz","vhnz","eiwh","opci","pqmx","irtc",
        "nvmu","whpq","qami","mght","nult","xvvq","ebfj","zizp","qofd","byyr","qtwi","amoz","jysx","qzhz","adio","zyvw",
        "xbfy","kemh","wcil","juvp","heeb","cotv","mxng","tgfg","mkxs","eimp","pzcj","hbyq","vqor","mtbn","ckyb","whhw",
        "whdk","hpra","jbjh","pnxp","bkpc","xgru","cjfn","fwdr","xfbx","azxj","cwrc","ipde","ibig","akwf","ivnd","qxhq",
        "dyeg","vtfa","rfvv","scrd","tmty","gdui","hcra","hrbv","basr","xigc","blzt","nkbs","quww","blme","pfqy","xtdb",
        "ooxq","gcbt","xqgx","vfae","nwvw","ipyt","efir","ayhe","ikek","pojc","jbvs","bryz","iodc","relu","ltip","xhgo",
        "jijc","vjpi","qeeq","tmiv","offb","xfzp","okxy","rfhf","gjnn","xlzr","cfgz","mxgo","zglg","hdqe","dcrf","uabp",
        "nveu","xwuw","ywct","byix","itsd","uwyr","wols","oiex","vykx","woqr","mvpx","icgt","mrmx","hsti","lajl","eouo",
        "fyns","gqbv","iebw","yanx","kpfu","wieb","nucf","lavj","cgeg","cjco","qyss","eghg","tjas","kkqw","npvf","ajkc",
        "wbnb","ogjr","zhmx","rjob","txnu","nxcv","ilbz","ajsw","bees","detf","gywe","grbn","ghok","wovc","brbh","ucyl",
        "dlvw","yeah","buxz","jbli","bnvb","jbvz","exaa","ekbh","nhot","wlmq","ghlv","lkne","xumz","ngwl","vtak","zita",
        "qthr","azig","luns","jvmi","rffd","eroy","nhhq","lojq","cuia","scec","adjs","kqma","itww","nmev","qmbd","sdfe",
        "wras","dzrq","duyj","rcds","qnqa","iauv","kvdr","joit","fjnl","escc","iomk","uzpy","icay","cgzy","edzf","xdfd",
        "gxyy","hrft","tjyh","dzob","xcvb","blhf","yzdb","fecr","nerh","xeex","tmhj","iuza","rlka","ueqe","svnu","exvi",
        "yikb","qvhr","mnch","yjng","pufx","gzqd","nlxz","quxf","kcba","etow","piee","omno","yyfl","pdmh","lppw","owlk",
        "gbrk","pmim","dqde","herm","igtt","ojem","kdmb","xiko","ablb","ykwd","oqmn","lxnd","sdbp","nykt","sgwa","rgkt",
        "sujq","evxt","teaq","blbp","ghmz","gwmt","uswt","nxai","jmhw","qakn","bxqh","lpge","tajd","vbyl","dpdf","icdl",
        "wsby","pral","swxp","jhyv","yrmx","qubx","gotc","uhoc","urgo","sdbn","vujo","gfbt","rgwo","mzgr","uamv","yzjq",
        "ypol","pwzy","tbtc","dxsu","adnu","hmbk","ogka","lebh","jndp","ftaq","sfal","gepn","jgpo","gyke","xwun","ferf",
        "hhmd","flgy","khnt","jqym","ypwr","vxfi","asnu","plnh","qkkc","bniq","rwpq","xrde","ilgd","ywwm","ienb","yqaw",
        "ftez","eusy","bqvw","lpin","vwej","hopz","wphz","nbci","tskz","ctmx","ihju","mrsz","xfsi","dfsv","dslj","zvgk",
        "ufvz","ogpr","uqhs","orgz","lxlt","qeys","nypp","xqqt","xtwn","dikx","cusx","ejga","oqgy","yych","yrxl","tono",
        "uhfp","lfcj","hava","floj","ywiv","vake","tvyr","kuia","rddl","inqj","fins","xmcn","psyd","pnwl","fjos","zpua",
        "mvjq","tziw","rmtz","ltyq","wasg","gbed","aibu","puxt","mzcz","pycr","zdlp","amdm","kcxk","ozaf","shrt","ppsu",
        "kqfz","jrnf","yejo","lmzn","uums","haja","htju","qmbx","rhid","eedb","sspk","uewy","ppee","gypg","dukf","vxbk",
        "ctbt","ypmh","paia","inwj","lnsd","dgxz","aacn","vxfk","xvmc","hcui","zunx","vtix","iyuk","dsio","hcgh","vrln",
        "maxo","pjwe","eeni","gcpi","hljt","kpsg","rqfn","aqju","olyw","posi","vmdd","rdgm","cyvg","shxd","vhoq","fjqo",
        "xgpx","mjdv","ziqn","nvin","yvlx","fpco","xuto","ltdo","revy","jlbq","dpvd","kocr","mpqb","hbrt","wvly","rctg",
        "dxoi","rzil","ubhe","wnjz","cbff","wuuw","ftvw","erja","lmwb","bkwu","mupu","nbzi","cgha","hjxe","hvjk","igzl",
        "rnch","pmod","ntck","mkui","dqbh","vogu","epzx","nsew","pkht","cthe","srok","jacs","gtjv","xttg","bbwr","krrf",
        "bspg","deva","bdyd","unvr","crou","mrvx","slcz","zxct","qbib","ktgn","otgf","wcmq","ytwi","oeec","ptjt","efuq",
        "zjbo","ielk","jspp","ybom","tihc","qtgv","mfrg","gunc","zqde","chax","rjdc","bfhj","pias","jnre","ntzl","ccvr",
        "vdtk","hgmb","ywkv","ubig","oywm","gbyq","tfxw","dlon","oswp","dabv","qnzl","qrpo","dcuy","fers","ajuq","zmzd",
        "uuah","wutt","xvjw","uvdc","hrbp","xiaq","ylxh","pmbd","mqeb","znyo","pagi","vnve","ryoa","yvgt","goyg","vazz",
        "uvjl","hgoi","xlov","qkhm","fmwe","akvn","ctjh","jdlt","erpf","sxxx","hirg","odgy","mnig","dgbs","bcdq","hrrr",
        "rswq","czpo","mmmt","cqgx","nsqd","vsvr","bbbg","mcrz","xtoa","rwtp","fppm","bjgd","pkwu","fgmg","dkuq","lrwy",
        "dlcn","yrke","yexf","lqfa","yvol","bdqi","hxmk","zxgs","fttc","fsye","kctk","yyou","jufa","mkpi","donb","bhfk",
        "iliu","vkts","rjod","ftit","rxpx","sffg","gqpc","jnyv","hzpt","anxi","xjtf","nblm","awoi","fgbu","qsaq","fewg",
        "trbo","tysi","gkrb","oust","mezx","cagk","wfkk","alxj","kqih","jgjv","agmy","tulj","cqsa","ncyi","kdaa","owcg",
        "zicc","uckj","kkar","jozy","eitq","qzfn","kdvx","zupg","zzmz","jezn","azzb","cclx","seov","wzuf","xxia","dypy",
        "cuct","zilu","tkop","thsf","ohcf","hxdf","phns","rekg","sryx","mzfr","xghr","vyss","wybt","qucp","krvd","rkbd",
        "vder","dgee","dmec","stgq","oxbf","fqsf","heut","ozbr","ixqn","nqgu","kufq","mcec","yggo","toun","ndfp","updc",
        "rcoi","umcn","qpma","ocoo","tnor","ecaq","kwfh","bjyy","rlwf","apwl","pkem","jovr","kopd","rnky","pqpm","bmfi",
        "yojg","agai","efuy","nkvm","wliu","hyzq","tyhu","fadk","hlui","qtig","bqli","ntue","ajgs","rfmn","lhwp","hhio",
        "ljml","bnyb","xmdm","qhye","szrb","flfz","rndu","wybe","wcio","uqpe","zqqi","bags","nnzy","bgoj","ladi","gtpa",
        "gzyo","vnox","kwek","iagq","fysx","vepy","ehva","stjz","ugjo","macf","bynl","ihuv","swlq","vdww","lktd","souz",
        "xtpa","fkrg","lpsp","ganf","rrqk","qxcc","mntq","npjb","xlon","saut","zgnf","rthc","kfmm","whfl","glbd","frua",
        "royk","psep","bept","laco","rfkq","oqhj","jesf","niss","dsrb","cwkx","ikje","mash","eurd","wbsk","rkjw","rulf",
        "xtvn","kolb","gmns","zbjf","hgge","sgqd","jzey","ozjs","qzdm","uodk","vxxi","hxiu","ymop","vnau","jqme","bacy",
        "hmrg","whnw","unio","icba","bixn","jdhi","cetg","fdvj","gjju","aaej","jpdo","iiix","vnfd","rfyn","rspv","cawh",
        "hfjy","igbt","qqug","reoy","aenr","mbtk","bgot","eecl","gxzy","meca","lrqs","hnst","kuui","muex","nusy","rgka",
        "lwra","flkq","ndwa","cfzn","strz","ilbd","rgxg","isfd","dsxq","zanx","ysmv","ixvp","gupj","jitm","quqx","wlwh",
        "oypv","wzii","upxe","mmkt","ehcv","kmlr","ifqi","aban","znwo","szjo","lxbg","okpw","wcpf","ixkf","fypa","mjjf",
        "gfng","gilq","nfwt","fitp","oumi","dchp","thvj","gtgo","dlot","tqcd","gvib","pqqi","xjte","riyb","dbxv","mfrb",
        "dewv","zdex","giuo","ylsk","ceqc","baah","odql","fcru","mkmz","seea","siyy","tvjc","bnie","irip","sirg","octh",
        "dvky","gnlr","yrgd","clwk","mcdr","fwwn","oveh","vcbf","ldcg","exet","krft","plpg","jrfb","ofxm","cyep","ubwm",
        "ktat","bbtd","xtko","qozu","lwut","ujen","nyxx","xbnm","ohdq","kzbg","wqjc","uqec","skyl","gtub","bkoq","hlgt",
        "twnr","xpds","wjle","fymi","qmlc","innz","fjhm","lzvl","zkpj","bysl","lulb","cszt","ossb","zobi","xusa","okqi",
        "yrmw","vuda","bosh","yimv","ljdx","sfwi","viwj","wrsp","vskp","yctz","jxjd","lwud","fkzo","fkqf","jths","sgvh",
        "iwps","osud","kojb","cedt","jlvv","vudh","xlog","zcik","licj","lrol","isey","aepy","uqty","bjkq","okbx","obih",
        "bgmc","tgpm","ieoh","hjsd","edoe","cxsp","tzsv","qomr","zfxz","ivoh","dmmh","hylz","trqx","macl","kufu","wpwl",
        "dgre","bhzw","bnur","haqp","dhte","ksni","idyj","vbal","minb","bhce","bphq","knpd","ggzq","urbe","fuwu","poip",
        "mmfg","dwjb","tgcm","mwny","pzcx","ongc","zuoy","wrvh","ypso","frkx","erch","lptp","gzeg","rluk","ckhr","jwzb",
        "fgro","zeec","xwtq","gajn","xdpr","sxzp","babk","qkks","yslx","mzpc","kycg","lvgp","rwor","ywwq","rqck","hhho",
        "gftu","fzop","ogrv","htfs","ibmf","xodm","gpnb","wpae","gdqp","qnlv","xugp","nxnh","bcdl","ysfb","ljws","vdpn",
        "iety","xywt","nvtm","viat","cdbh","jtev","kerb","vazi","cpdk","tmtl","fbgt","poqq","naje","cwzs","alhi","mbwn",
        "kbcm","uxxt","zpba","thao","jonm","eunt","yhqj","gyyg","gmhp","bmtl","jgal","oilw","ucet","ihsq","vdph","evus",
        "obqw","raff","cxtt","djdh","gdxw","ncza","fapv","qzrt","vhir","jfby","ogmk","fuin","yijf","oyqb","wryi","uhty",
        "yqvl","ndad","ytys","dqyz","ojsm","qtcx","fcti","lwzb","qaye","qott","hohh","lhag","xqok","mijn","yunk","wmma",
        "paxw","kxld","jgwy","qcun","mruc","tpjv","xzfl","wtxa","gtta","airy","cfzk","zuuk","lhcu","ktvb","fimb","kzrn",
        "jgsc","crrt","kdxw","xptp","fdrq","ffrr","jkuu","cbmz","aodz","hurb","pexf","xftd","azqn","ypfv","nuxv","supw",
        "zibp","itlv","drmf","xeic","xbrk","skbp","pijt","khka","vwrq","qsse","clyf","otvu","pgza","nodt","tntc","ljkv",
        "mqxb","vkqu","xdii","kyti","gioi","dwuy","pfjz","evsp",
};
int global_keys_count = ARRAY_SIZE(global_keys);

class HashtableSpscOpGetFixture : public benchmark::Fixture {
private:
    hashtable_spsc_t *_hashtable = nullptr;
    char **_keys = (char**)global_keys;
    int _keys_count = global_keys_count;
public:
    hashtable_spsc_t *GetHashtable() {
        return this->_hashtable;
    }

    void SetUp(benchmark::State& state) override {
        this->_hashtable = hashtable_spsc_new(
                100,
                24,
                true);

        for (int index = 0; index < this->_keys_count; index++) {
            char *key_dup = (char*)xalloc_alloc(strlen(_keys[index]) + 1);
            strcpy(key_dup, _keys[index]);
            key_dup[strlen(_keys[index])] = '\0';

            if (!hashtable_spsc_op_try_set_cs(
                    this->_hashtable,
                    key_dup,
                    strlen(_keys[index]),
                    _keys[index])) {
                // Move back the pointer of the keys array
                index--;

                // Resize the hashtable
                uint32_t current_size = this->_hashtable->buckets_count_real;
                this->_hashtable = hashtable_spsc_upsize(this->_hashtable);
                if (!hashtable_spsc_op_try_set_cs(
                        this->_hashtable,
                        key_dup,
                        strlen(_keys[index]),
                        _keys[index])) {
                    state.SkipWithError("Failed to set key in hashtable");
                }
            }
        }

        // Validate that all the keys are in the hashtable and can be found
        for (int index = 0; index < this->_keys_count; index++) {
            char *key = (char*)global_keys[index];
            size_t key_length = strlen(global_keys[index]);
            if (hashtable_spsc_op_get_cs(
                    this->_hashtable,
                    key,
                    key_length) != key) {
                state.SkipWithError("Failed to find key in hashtable");
            }
        }
    }

    void TearDown(const ::benchmark::State &state) override {
        hashtable_spsc_free(this->_hashtable);
    }
};

class HashtableSpscOpGetKeyAsIntFixture : public benchmark::Fixture {
private:
    hashtable_spsc_t *_hashtable = nullptr;
    char **_keys = (char**)global_keys;
    int _keys_count = global_keys_count;
public:
    hashtable_spsc_t *GetHashtable() {
        return this->_hashtable;
    }

    void SetUp(benchmark::State& state) override {
        this->_hashtable = hashtable_spsc_new(
                100,
                24,
                false);

        for (int index = 0; index < this->_keys_count; index++) {
            if (!hashtable_spsc_op_try_set_by_hash_and_key_uint32(
                    this->_hashtable,
                    *(uint32_t*)_keys[index],
                    *(uint32_t*)_keys[index],
                    _keys[index])) {
                // Move back the pointer of the keys array
                index--;

                // Resize the hashtable
                uint32_t current_size = this->_hashtable->buckets_count_real;
                this->_hashtable = hashtable_spsc_upsize(this->_hashtable);
                if (!hashtable_spsc_op_try_set_by_hash_and_key_uint32(
                        this->_hashtable,
                        *(uint32_t*)_keys[index],
                        *(uint32_t*)_keys[index],
                        _keys[index])) {
                    state.SkipWithError("Failed to set key in hashtable");
                }
            }
        }

        // Validate that all the keys are in the hashtable and can be found
        for (int index = 0; index < this->_keys_count; index++) {
            if (hashtable_spsc_op_get_by_hash_and_key_uint32(
                    this->_hashtable,
                    *(uint32_t*)_keys[index],
                    *(uint32_t*)_keys[index]) != _keys[index]) {
                state.SkipWithError("Failed to find key in hashtable");
            }
        }
    }

    void TearDown(const ::benchmark::State &state) override {
        hashtable_spsc_free(this->_hashtable);
    }
};

BENCHMARK_DEFINE_F(HashtableSpscOpGetFixture, FindTokenInHashtableWorstCase1Benchmark)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_cs(
                this->GetHashtable(),
                "non-existing",
                strlen("non-existing")));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCaseCIBenchmark)(benchmark::State& state) {
    char *key = (char*)global_keys[3000];
    size_t key_length = strlen(global_keys[3000]);
    hashtable_spsc_t *hashtable = this->GetHashtable();

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_ci(
                hashtable,
                key,
                key_length));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCaseCSBenchmark)(benchmark::State& state) {
    char *key = (char*)global_keys[3000];
    size_t key_length = strlen(global_keys[3000]);
    hashtable_spsc_t *hashtable = this->GetHashtable();

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_cs(
                hashtable,
                key,
                key_length));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey1Benchmark)(benchmark::State& state) {
    char *key = (char*)global_keys[999];
    uint32_t key_uint32 = *(uint32_t *)key;

    hashtable_spsc_t *hashtable = this->GetHashtable();

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_by_hash_and_key_uint32(
                hashtable,
                key_uint32,
                key_uint32));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey2Benchmark)(benchmark::State& state) {
    char *key = (char*)global_keys[1999];
    uint32_t key_uint32 = *(uint32_t *)key;

    hashtable_spsc_t *hashtable = this->GetHashtable();

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_by_hash_and_key_uint32(
                hashtable,
                key_uint32,
                key_uint32));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey3Benchmark)(benchmark::State& state) {
    char *key = (char*)global_keys[3000];
    uint32_t key_uint32 = *(uint32_t *)key;

    hashtable_spsc_t *hashtable = this->GetHashtable();

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_by_hash_and_key_uint32(
                hashtable,
                key_uint32,
                key_uint32));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey4Benchmark)(benchmark::State& state) {
    char *key = (char*)global_keys[3999];
    uint32_t key_uint32 = *(uint32_t *)key;

    hashtable_spsc_t *hashtable = this->GetHashtable();

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_by_hash_and_key_uint32(
                hashtable,
                key_uint32,
                key_uint32));
    }
}

BENCHMARK_DEFINE_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey5Benchmark)(benchmark::State& state) {
    char *key = (char*)global_keys[4999];
    uint32_t key_uint32 = *(uint32_t *)key;

    hashtable_spsc_t *hashtable = this->GetHashtable();

    for (auto _ : state) {
        benchmark::DoNotOptimize(hashtable_spsc_op_get_by_hash_and_key_uint32(
                hashtable,
                key_uint32,
                key_uint32));
    }
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b->Iterations(100000000);
}

BENCHMARK_REGISTER_F(HashtableSpscOpGetFixture, FindTokenInHashtableWorstCase1Benchmark)
    ->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCaseCIBenchmark)
    ->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetFixture, FindTokenInHashtableAvgCaseCSBenchmark)
    ->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey1Benchmark)
->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey2Benchmark)
->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey3Benchmark)
->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey4Benchmark)
->Apply(BenchArguments);
BENCHMARK_REGISTER_F(HashtableSpscOpGetKeyAsIntFixture, FindTokenInHashtableBypassHashAndKey5Benchmark)
->Apply(BenchArguments);
