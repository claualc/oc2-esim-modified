# Modifications

- Able to read GNB_ID env variables: 1, 2 ,3 and 4 to set different meiads to the gnbs
- Action Definition ids:
    - action definiton id 1: REPORT service (Sets periodic timer)
    - action definiton id 2: INSERT service 
- Message sizes up to 9000/8 bytes. ( e2sm_examples/kpm_e2sm/src/kpm/bs_connector.cpp 101)
    
# Build
Run `build_e2sim.sh`, add `-i` to install all required dependencies and build JSON library from source.  

Note: cmake 3.14 or newer is needed. [Update cmake on Ubuntu 16.04](https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line)

# RUN
Run `run_e2sim.sh`# oc2-esim-modified
