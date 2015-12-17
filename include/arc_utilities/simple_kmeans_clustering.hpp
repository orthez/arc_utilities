#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <chrono>
#include <random>
#include <Eigen/Geometry>

#ifndef SIMPLE_KMEANS_CLUSTERING_HPP
#define SIMPLE_KMEANS_CLUSTERING_HPP

namespace simple_kmeans_clustering
{
    class SimpleKMeansClustering
    {
    private:

        SimpleKMeansClustering() {}

        template<typename Datatype, typename Allocator=std::allocator<Datatype>>
        static u_int32_t GetClosestCluster(const Datatype& datapoint, std::function<double(const Datatype&, const Datatype&)>& distance_fn, const std::vector<Datatype, Allocator>& cluster_centers)
        {
            int64_t best_label = -1;
            double best_distance = INFINITY;
            for (size_t cluster = 0; cluster < cluster_centers.size(); cluster++)
            {
                const Datatype& cluster_center = cluster_centers[cluster];
                const double distance = distance_fn(cluster_center, datapoint);
                if (distance < best_distance)
                {
                    best_distance = distance;
                    best_label = (int64_t)cluster;
                }
            }
            assert(best_label >= 0);
            return (u_int32_t)best_label;
        }

        template<typename Datatype, typename Allocator=std::allocator<Datatype>>
        static std::vector<u_int32_t> PerformSingleClusteringIteration(const std::vector<Datatype, Allocator>& data, std::function<double(const Datatype&, const Datatype&)>& distance_fn, const std::vector<Datatype, Allocator>& cluster_centers)
        {
            std::vector<u_int32_t> cluster_labels(data.size());
            for (size_t idx = 0; idx < data.size(); idx++)
            {
                const Datatype& datapoint = data[idx];
                const u_int32_t label = GetClosestCluster(datapoint, distance_fn, cluster_centers);
                cluster_labels[idx] = label;
            }
            return cluster_labels;
        }

        template<typename Datatype, typename Allocator=std::allocator<Datatype>>
        static std::vector<Datatype, Allocator> ComputeClusterCenters(const std::vector<Datatype, Allocator>& data, const std::vector<u_int32_t>& cluster_labels, std::function<Datatype(const std::vector<Datatype, Allocator>&)>& average_fn, const u_int32_t num_clusters)
        {
            assert(data.size() == cluster_labels.size());
            // Separate the datapoints into their clusters
            std::vector<std::vector<Datatype, Allocator>> clustered_data(num_clusters);
            for (size_t idx = 0; idx < data.size(); idx++)
            {
                const Datatype& datapoint = data[idx];
                const u_int32_t label = cluster_labels[idx];
                clustered_data[label].push_back(datapoint);
            }
            // Compute the center of each cluster
            std::vector<Datatype, Allocator> cluster_centers(num_clusters);
            for (u_int32_t cluster = 0; cluster < num_clusters; cluster++)
            {
                const std::vector<Datatype, Allocator>& cluster_data = clustered_data[cluster];
                cluster_centers[cluster] = average_fn(cluster_data);
            }
            return cluster_centers;
        }

        static bool CheckForConvergence(const std::vector<u_int32_t>& old_labels, const std::vector<u_int32_t>& new_labels)
        {
            assert(old_labels.size() == new_labels.size());
            for (size_t idx = 0; idx < old_labels.size(); idx++)
            {
                const u_int32_t old_label = old_labels[idx];
                const u_int32_t new_label = new_labels[idx];
                if (old_label != new_label)
                {
                    return false;
                }
            }
            return true;
        }

    public:

        template<typename Datatype, typename Allocator=std::allocator<Datatype>>
        static std::vector<u_int32_t> Cluster(const std::vector<Datatype, Allocator>& data, std::function<double(const Datatype&, const Datatype&)>& distance_fn, std::function<Datatype(const std::vector<Datatype, Allocator>&)>& average_fn, const u_int32_t num_clusters, const bool do_preliminary_clustering=false)
        {
            assert(data.size() > 0);
            assert(num_clusters > 0);
            if (num_clusters == 1)
            {
                std::cerr << "[K-means clustering] Provided num_clusters = 1, returning default labels for cluster 0" << std::endl;
                return std::vector<u_int32_t>(data.size(), 0u);
            }
            // Prepare an RNG for cluster initialization
            unsigned long seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            std::mt19937_64 prng(seed);
            std::uniform_int_distribution<size_t> initialization_distribution(0u, data.size() - 1);
            // Initialize cluster centers
            std::vector<Datatype, Allocator> cluster_centers;
            // Make sure we have enough datapoints to do meaningful preliminary clustering
            bool enable_preliminary_clustering = do_preliminary_clustering;
            if (enable_preliminary_clustering)
            {
                const size_t subset_size = (size_t)ceil((double)data.size() * 0.1);
                if (subset_size >= (num_clusters * 5))
                {
                    enable_preliminary_clustering = true;
                    std::cerr << "[K-means clustering] Preliminary clustering enabled, using subset of " << subset_size << " datapoints from " << data.size() << " total" << std::endl;
                }
                else
                {
                    enable_preliminary_clustering = false;
                    std::cerr << "[K-means clustering] Preliminary clustering disabled as input data is too small w.r.t. number of clusters" << std::endl;
                }
            }
            if (enable_preliminary_clustering)
            {
                // Select a random 10% of the input data
                const size_t subset_size = (size_t)ceil((double)data.size() * 0.1);
                // This makes sure we don't get duplicates
                std::map<size_t, u_int8_t> index_map;
                while (index_map.size() < subset_size)
                {
                    const size_t random_index = initialization_distribution(prng);
                    index_map[random_index] = 1u;
                }
                std::vector<Datatype, Allocator> random_subset;
                random_subset.reserve(subset_size);
                for (auto itr = index_map.begin(); itr != index_map.end(); ++itr)
                {
                    if (itr->second == 1u)
                    {
                        const size_t random_index = itr->first;
                        const Datatype& random_element = data[random_index];
                        random_subset.push_back(random_element);
                    }
                }
                assert(random_subset.size() == subset_size);
                // Run clustering on the subset
                std::vector<u_int32_t> random_subset_labels = Cluster(random_subset, distance_fn, average_fn, num_clusters, false);
                // Now we use the centers of the clusters to form the cluster centers
                cluster_centers = ComputeClusterCenters(random_subset, random_subset_labels, average_fn, num_clusters);
            }
            else
            {
                // This makes sure we don't get duplicates
                std::map<size_t, u_int8_t> index_map;
                while (index_map.size() < num_clusters)
                {
                    const size_t random_index = initialization_distribution(prng);
                    index_map[random_index] = 1u;
                }
                cluster_centers.reserve(num_clusters);
                for (auto itr = index_map.begin(); itr != index_map.end(); ++itr)
                {
                    if (itr->second == 1u)
                    {
                        const size_t random_index = itr->first;
                        const Datatype& random_element = data[random_index];
                        cluster_centers.push_back(random_element);
                    }
                }
                assert(cluster_centers.size() == num_clusters);
            }
            assert(cluster_centers.size() == num_clusters);
            // Run the first iteration of clustering
            std::vector<u_int32_t> cluster_labels = PerformSingleClusteringIteration(data, distance_fn, cluster_centers);
            bool converged = false;
            u_int32_t iteration = 1u;
            while (!converged)
            {
                // Update cluster centers
                cluster_centers = ComputeClusterCenters(data, cluster_labels, average_fn, num_clusters);
                // Cluster with the new centers
                std::vector<u_int32_t> new_cluster_labels = PerformSingleClusteringIteration(data, distance_fn, cluster_centers);
                // Check for convergence
                converged = CheckForConvergence(cluster_labels, new_cluster_labels);
                cluster_labels = new_cluster_labels;
                iteration++;
            }
            std::cerr << "[K-means clustering] Clustering converged after " << iteration << " iterations" << std::endl;
            return cluster_labels;
        }
    };
}
#endif // SIMPLE_KMEANS_CLUSTERING_HPP