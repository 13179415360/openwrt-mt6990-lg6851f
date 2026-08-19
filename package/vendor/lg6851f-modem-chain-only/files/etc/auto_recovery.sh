#!/bin/sh                                                      
rm -rf /dev/thermal_core_socket
/etc/init.d/scd restart                                         
/etc/init.d/led_ctrld.init restart                              
/etc/init.d/mtk_netagent restart                                
/etc/init.d/thermal_core restart                                
/etc/init.d/mtk_agpsd restart                                   
/etc/init.d/ql_ril_service restart                              
/etc/init.d/atcid restart                                       
/etc/init.d/ql_netd restart                                     
/etc/init.d/mdlogger restart                                    
                                                                
i=0 
until [ ! $i -lt 21 ]
do                                                              
        ifconfig ccmni$i down                                   
        echo "ccmni$i  down ok"      
        i=`expr $i + 1`                          
done                                                            
                                                                
mipc_wan_cli --at_cmd at+cfun=0                                 
                                                                
sleep 2                                                         
                                  
mipc_wan_cli --at_cmd at+cfun=1         
                                        
ubus send modem_ee                      
                                        
etc/init.d/mipc_submonitor.init restart