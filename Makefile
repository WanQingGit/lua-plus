#trunk
DEBUG	= 0
#多个目录用空格分开
DIR_SRC	= src
DIR_TEST= test
DIR_INC = ./include 
DIR_OBJ	= ./bin
DIR_LIB	= #../common/bin ../parser/bin
TARGET = lua luac#有多个文件含main函数，就有多个目标（文件名）
TARGET_SO =lua
TEST	= base#list rbtreeTest mro bytes maptest logtest iotest
EXCLUDE_OBJ=mem_pool 
EXCLUDE_TEST_OBJ=testbase 
CC := gcc -std=gnu99
CPP	:= g++
LIBS :=m #pthread  QUtil QParser
LIBS_STATIC :=	#–static –lmysqlclient
LDFLAGS :=
DEFINES := #MEM_DEBUG

#搜索路径
DIRS = $(shell find $(DIR_SRC) -maxdepth 2 -type d)

#源文件名带路径	$(wildcard ./*.c ./snap7/*.c)
SRCS_CPP = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cpp))
SRCS_C = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
SOURCE := $(SRCS_CPP) $(SRCS_C)
EXCLUDE_OBJ	:=$(addprefix $(DIR_SRC)/,$(addsuffix .o, $(EXCLUDE_OBJ)))
OBJS :=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o, ${SOURCE}))
OBJS :=$(filter-out $(EXCLUDE_OBJ),$(OBJS))
OBJS_OUT:=$(OBJS:$(DIR_SRC)/%=$(DIR_OBJ)/%)

DIRS = $(shell find $(DIR_TEST) -maxdepth 2 -type d)
TEST_CPP = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cpp))
TEST_C = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
SRC_TEST := $(TEST_CPP) $(TEST_C)
EXCLUDE_TEST_OBJ	:=$(addprefix $(DIR_TEST)/,$(addsuffix .o, $(EXCLUDE_TEST_OBJ)))
OBJS_TEST :=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o, ${SRC_TEST}))
OBJS_TEST :=$(filter-out $(EXCLUDE_TEST_OBJ),$(OBJS_TEST))
OBJS_TEST_OUT:=$(OBJS_TEST:$(DIR_TEST)/%=$(DIR_OBJ)/$(DIR_TEST)/%)

DEPS	:= $(patsubst %.o,%.d,$(OBJS_OUT))
#DEPS	:=$(objects:.o=.d)  

FILE_MK	:= $(wildcard $(DIR_SRC)/*.mk)
ifneq ($(strip $(FILE_MK)),)
-include $(FILE_MK)
endif
ifeq ($(DEBUG), 0) 
 CFLAGS+= -O2
# CFLAGS+= -pg
# DEFINES+=BAN_POOL 
# DEFINES+=INSTR_GOTO 
else
 CFLAGS+= -O0 -g3 -Wall 
 CFLAGS+=  -pg
# DEFINES+=BAN_POOL
# DEFINES+=LUA_PRINT
# DEFINES+= QDEBUG
endif

CFLAGS +=-fgnu89-inline $(addprefix -D,$(DEFINES)) -fPIC -shared $(addprefix -I,$(DIR_INC))
CXXFLAGS:= $(CFLAGS)

#shell放入preinstall有问题，返回的数值会当作命令被执行
exist = $(shell if [ -d $(DIR_OBJ) ]; then echo ""; else mkdir -p $(DIR_OBJ); fi;)
preinstall:
	@echo $(exist)


all: $(TARGET)
#$(notdir src/foo.chacks) 返回值是“foo.chacks”
#$(TARGET) :objs=$(notdir outObj))
$(TARGET) :exclude=$(filter-out $@,$(TARGET))
$(TARGET) :include=$(DIR_OBJ)/$@.o
$(TARGET) :preinstall lib $(include) #$^全部依赖 $<第一个依赖 $@目标
	$(CC) $(LDFLAGS) -fPIC $(filter-out $(exclude:%=$(DIR_OBJ)/%.o),$(OBJS_OUT))   $(addprefix -L,$(DIR_LIB))  -Wl,-Bstatic $(addprefix -l,$(LIBS_STATIC))  -Wl,-Bdynamic $(addprefix -l,$(LIBS))  -o $(DIR_OBJ)/$@ 
#链接库
#	$(CC) $(include) $(LDFLAGS) -fPIC $(addprefix -L,$(DIR_LIB))  $(addprefix -L,$(DIR_OBJ)) -Wl,-Bstatic $(addprefix -l,$(LIBS_STATIC)) -Wl,-Bdynamic $(addprefix -l,$(LIBS)) $(addprefix -l,$(TARGET_SO)) -o $(DIR_OBJ)/$@  

test:$(TEST)
$(TEST) :exclude=$(filter-out $@,$(TEST))
$(TEST) :preinstall $(OBJS_TEST)	#$^全部依赖 $<第一个依赖 $@目标
#	$(CC) -shared $(OBJS) $(LDFLAGS) $(LIBS) -o $(DIR_OBJ)$(outSo) #动态库
#动态库
#	$(CC) -shared $(filter-out $(exclude:%=$(DIR_OBJ)/%.o),$(OBJS)) $(LDFLAGS) $(LIBS) -o $(DIR_OBJ)/$@.so
#可执行文件
	$(CC) $(filter-out $(exclude:%=$(DIR_OBJ)/$(DIR_TEST)/%.o),$(OBJS_TEST_OUT)) $(LDFLAGS) -fPIC $(addprefix -L,$(DIR_OBJ)) $(addprefix -L,$(DIR_LIB)) -Wl,-Bdynamic $(addprefix -l,$(TARGET_SO)) $(addprefix -l,$(LIBS)) -o $(DIR_OBJ)/$@  
#	$(CC) $^ $(LDFLAGS) $(LIBS) -o $(DIR_OBJ)/$@ 
#	@$(DIR_OBJ)/$@ #编译后立即执行

lib:opt=-fPIC -shared
#lib:outObj=$(addprefix $(DIR_OBJ)/,$(OBJS:$(DIR_SRC)/%=%))
lib:outObj=$(OBJS_OUT)
lib:include=$(filter-out $(TARGET:%= $(DIR_OBJ)/%.o),$(outObj))
lib:preinstall $(OBJS_OUT)
#	@echo $(include:%=$(DIR_OBJ)/%.o)
#	$(CC) -shared $(include:%=$(DIR_OBJ)/%.o) $(LDFLAGS) $(addprefix -L,$(DIR_LIB)) -Wl,-Bstatic $(addprefix -l,$(LIBS_STATIC)) -Wl,-Bdynamic $(addprefix -l,$(LIBS)) -o $(DIR_OBJ)/lib$(TARGET_SO).so 
	$(CC) -shared $(include) $(LDFLAGS) $(addprefix -L,$(DIR_LIB)) -Wl,-Bstatic $(addprefix -l,$(LIBS_STATIC)) -Wl,-Bdynamic $(addprefix -l,$(LIBS)) -o $(DIR_OBJ)/lib$(TARGET_SO).so

clean:
	rm -rf $(OBJS_TEST_OUT) $(OBJS_OUT) $(DEPS) $(addprefix $(DIR_OBJ)/,$(TEST)) $(addprefix $(DIR_OBJ)/,$(TARGET)) $(addprefix $(DIR_OBJ)/lib,$(TARGET_SO:%=%.so))
	@echo ' '



.PHONY: all clean preinstall

# Each subdirectory must supply rules for building sources it contributes

#%.o:out=$(DIR_OBJ)/$(@:$(DIR_SRC)/%=%)
$(DIR_OBJ)/%.o: $(DIR_SRC)/%.cpp
	@echo $(shell if [ -d $(dir $(out)) ]; then echo ""; else mkdir -p $(dir $(out)); fi;)
	@echo 'Building c++ file: $< $@'
	$(CPP) $(CXXFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.c
	@echo $(shell if [ -d $(dir "$<") ]; then echo ""; else mkdir -p $(dir "$<"); fi;)
	@echo 'Building c file: $< $@'
	$(CC) $(CFLAGS)  -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<" # 
	@echo 'Finished building: $<'
	@echo ' '

# All Target $< $^ $(subst info,INFO,$@)  
info info2: one.j two.j  
	@echo $(OBJS_OUT)
	@echo 'FILE_MK $<'
	@echo $< $@

%.j:
	@echo 'test $@' 
Huffman:src/Huffman.o
	$(CPP) $(^:$(DIR_SRC)%=$(DIR_OBJ)%) -o $(DIR_OBJ)/$@  
echo:outObj=$(DIR_OBJ)$(OBJS:$(DIR_SRC)%=%)#错误示范
echo:notdir_=$(notdir $(outObj))
echo:dir_=$(dir $(DIR_INC))
echo: #调试时显示一些变量的值  
#	@echo SOURCE=$(SOURCE),INC=$(addprefix -I,$(DIR_INC))
	@echo OBJS_TEST=$(OBJS_TEST)
	@echo OBJS_TEST_OUT=$(OBJS_TEST_OUT)
	@echo OBJS=$(OBJS)
	@echo f_notdir=$(notdir_)
	@echo f_dir=$(dir_)
	@echo exclude=$(exclude),include=$(include)
	@echo alloutOBJS=$(addprefix $(DIR_OBJ),$(OBJS:$(DIR_SRC)%=%))
	@echo EXCLUDE_OBJ=$(EXCLUDE_OBJ)
	@echo OBJS_OUT=$(OBJS_OUT)
	@echo filtOutOBJS=$(filter-out $(EXCLUDE_OBJ),$(OBJS:$(DIR_SRC)%=%))
