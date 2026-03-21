################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/BSP/Components/vl53l8cx/porting/platform.c 

OBJS += \
./Drivers/BSP/Components/vl53l8cx/porting/platform.o 

C_DEPS += \
./Drivers/BSP/Components/vl53l8cx/porting/platform.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/BSP/Components/vl53l8cx/porting/%.o Drivers/BSP/Components/vl53l8cx/porting/%.su Drivers/BSP/Components/vl53l8cx/porting/%.cyclo: ../Drivers/BSP/Components/vl53l8cx/porting/%.c Drivers/BSP/Components/vl53l8cx/porting/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../X-CUBE-TOF1/Target -I../Drivers/BSP/Components/Common -I../Drivers/BSP/53L8A1 -I../Drivers/BSP/Components/vl53l8cx/modules -I../Drivers/BSP/Components/vl53l8cx/porting -I../Drivers/BSP/Components/vl53l8cx -I../NEAI_Lib/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-BSP-2f-Components-2f-vl53l8cx-2f-porting

clean-Drivers-2f-BSP-2f-Components-2f-vl53l8cx-2f-porting:
	-$(RM) ./Drivers/BSP/Components/vl53l8cx/porting/platform.cyclo ./Drivers/BSP/Components/vl53l8cx/porting/platform.d ./Drivers/BSP/Components/vl53l8cx/porting/platform.o ./Drivers/BSP/Components/vl53l8cx/porting/platform.su

.PHONY: clean-Drivers-2f-BSP-2f-Components-2f-vl53l8cx-2f-porting

