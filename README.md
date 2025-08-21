# Uno: A One-Stop Solution for Inter- and Intra-Datacenter Congestion Control and Reliable Connectivity
Cloud computing and AI workloads are driving unprecedented demand for efficient communication within and across datacenters. However, the coexistence of intra- and inter-datacenter traffic within datacenters plus the disparity between the RTTs of intra- and inter-datacenter networks complicates congestion management and traffic routing. Particularly, faster congestion responses of intra-datacenter traffic causes rate unfairness when competing with slower inter-datacenter flows. Additionally, inter-datacenter messages suffer from slow loss recovery and, thus, require reliability. Existing solutions overlook these challenges and handle inter- and intra-datacenter congestion with separate control loops or at different granularities. We propose Uno, a unified system for both inter- and intra-DC environments that integrates a transport protocol for rapid congestion reaction and fair rate control with a load balancing scheme that combines erasure coding and adaptive routing. Our findings show that Uno significantly improves the completion times of both inter- and intra-DC flows compared to state-of-the-art methods such as Gemini.

# Installing Requirements
First install the required Python packages by running from the root directory of the repository the following command:
```
./sc25_pkg_install.sh
```

Compile with the following instruction. To do so, we recommend running the following command line from the ```/sim``` directory (feel free to change the number of jobs being run in parallel).

```
make clean && cd datacenter/ && make clean && cd .. && make -j 8 && cd datacenter/ && make -j 8 && cd ..
```

# Testing the artifacts
To test the artifacts we provide a series of bash files that can be run from the root directory of the project. To reproduce most of the key results of the paper without having to wait for too long, we provide a bash script that runs quickly (1-2 hrs expected runtime) and generates most of the results present in the paper. To run it, use:
```
./sc25_quick_validation.sh
```
Results are automatically generated in the ```artifact_results/``` folder.

# Running the Docker container
If running the code from the provided Docker container (https://doi.org/10.5281/zenodo.15916080), these steps are necessary:
```
sudo docker load -i uno_container.tar.gz
sudo docker run -it uno:1.0
```
If viewing images such as plots inside the container is problematic, it is possible to transfer that file to the host machine using:
```
sudo docker cp <containerId>:/file/path/within/container /host/path/target
```
It is possible to obtain the ```<containerId>``` by running
```
sudo docker ps
```