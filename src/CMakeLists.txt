file(GLOB libsrc *.cpp)
add_library(libcoro4spdk STATIC ${libsrc})
set(spdk_link_options -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fuse-ld=bfd)
set(spdk_libs "-L${spdk_root}/build/lib -Wl,--whole-archive -Wl,--no-as-needed -lspdk_bdev_malloc -lspdk_bdev_null -lspdk_bdev_nvme -lspdk_bdev_passthru -lspdk_bdev_lvol -lspdk_bdev_raid -lspdk_bdev_error -lspdk_bdev_gpt -lspdk_bdev_split -lspdk_bdev_delay -lspdk_bdev_zone_block -lspdk_blobfs_bdev -lspdk_blobfs -lspdk_blob_bdev -lspdk_lvol -lspdk_blob -lspdk_nvme -lspdk_bdev_aio -lspdk_bdev_ftl -lspdk_ftl -lspdk_bdev_virtio -lspdk_virtio -lspdk_vfio_user -lspdk_accel_ioat -lspdk_ioat -lspdk_scheduler_dynamic -lspdk_env_dpdk -lspdk_scheduler_dpdk_governor -lspdk_scheduler_gscheduler -lspdk_sock_posix -lspdk_event -lspdk_event_bdev -lspdk_bdev -lspdk_notify -lspdk_event_accel -lspdk_accel -lspdk_dma -lspdk_event_vmd -lspdk_vmd -lspdk_event_sock -lspdk_sock -lspdk_event_iobuf -lspdk_init -lspdk_thread -lspdk_trace -lspdk_rpc -lspdk_jsonrpc -lspdk_json -lspdk_util -lspdk_log -lspdk_keyring -Wl,--no-whole-archive")
set(spdk_env_libs "${spdk_root}/build/lib/libspdk_env_dpdk.a -Wl,--whole-archive ${spdk_root}/dpdk/build/lib/librte_bus_pci.a ${spdk_root}/dpdk/build/lib/librte_cryptodev.a ${spdk_root}/dpdk/build/lib/librte_dmadev.a ${spdk_root}/dpdk/build/lib/librte_eal.a ${spdk_root}/dpdk/build/lib/librte_log.a ${spdk_root}/dpdk/build/lib/librte_ethdev.a ${spdk_root}/dpdk/build/lib/librte_hash.a ${spdk_root}/dpdk/build/lib/librte_kvargs.a ${spdk_root}/dpdk/build/lib/librte_mbuf.a ${spdk_root}/dpdk/build/lib/librte_mempool.a ${spdk_root}/dpdk/build/lib/librte_mempool_ring.a ${spdk_root}/dpdk/build/lib/librte_net.a ${spdk_root}/dpdk/build/lib/librte_pci.a ${spdk_root}/dpdk/build/lib/librte_power.a ${spdk_root}/dpdk/build/lib/librte_rcu.a ${spdk_root}/dpdk/build/lib/librte_ring.a ${spdk_root}/dpdk/build/lib/librte_telemetry.a ${spdk_root}/dpdk/build/lib/librte_vhost.a -Wl,--no-whole-archive -lbsd -lnuma -ldl")
set(spdk_sys_libs "-L${spdk_root}/isa-l/.libs -lisal -L${spdk_root}/isa-l-crypto/.libs -lisal_crypto -pthread -lrt -luuid -lssl -lcrypto -lm -laio -lfuse3")
set(spdk_link_libraries "${spdk_libs} ${spdk_env_libs} ${spdk_sys_libs}")

# set linker options before object file list
target_link_options(libcoro4spdk PRIVATE ${spdk_link_options})

# set linker options after object file list i.e. dependencies
target_link_libraries(libcoro4spdk PRIVATE ${spdk_link_libraries})
target_include_directories(libcoro4spdk PRIVATE ${spdk_root}/include)
target_include_directories(libcoro4spdk PUBLIC include)
