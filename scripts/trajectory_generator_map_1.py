#!/usr/bin/env python3
"""
Simplified Trajectory Generator for Map 1
Generates straight-line trajectories between waypoints for radius-based collision domain simulation.

Usage:
    python3 trajectory_generator_map_1.py [--config <config_file>] [--vehicles <num>] [--duration <seconds>] [--output-dir <path>]
"""

import argparse
import csv
import json
import math
import random
import os
from typing import Tuple, List, Dict, Any

class SimpleMapConfig:
    """Simplified configuration loader for waypoint-based trajectories."""
    
    def __init__(self, config_file: str):
        with open(config_file, 'r') as f:
            self.config = json.load(f)
    
    @property
    def rsu(self) -> Dict[str, Any]:
        return self.config['rsu']
    
    @property
    def vehicles(self) -> Dict[str, Any]:
        return self.config['vehicles']
    
    @property
    def simulation(self) -> Dict[str, Any]:
        return self.config['simulation']
    
    @property
    def waypoints(self) -> List[Dict[str, Any]]:
        return self.config['waypoints']
    
    @property
    def routes(self) -> List[Dict[str, Any]]:
        return self.config['routes']
    
    @property
    def logging(self) -> Dict[str, str]:
        return self.config['logging']

class SimpleTrajectoryGenerator:
    """Generates simple straight-line trajectories between waypoints."""
    
    def __init__(self, config: SimpleMapConfig, duration_seconds: int = None, update_interval_ms: int = None):
        self.config = config
        self.duration_ms = (duration_seconds or config.simulation['duration_s']) * 1000
        self.update_interval_ms = update_interval_ms or config.simulation['update_interval_ms']
        self.timestamps = list(range(0, self.duration_ms + 1, self.update_interval_ms))
        
        # Build waypoint lookup
        self.waypoints = {wp['name']: wp for wp in config.waypoints}
        self.routes = config.routes
        
        # Vehicle configuration
        self.vehicle_speed_kmh = config.vehicles['speed_kmh']
        self.vehicle_speed_ms = self.vehicle_speed_kmh / 3.6  # Convert to m/s
        
    def generate_vehicle_trajectory(self, vehicle_id: int) -> List[Tuple[int, float, float]]:
        """Generate a straight-line trajectory following a random route."""
        trajectory = []
        
        # Pick a random route
        route = random.choice(self.routes)
        route_waypoints = [self.waypoints[wp_name] for wp_name in route['waypoints']]
        
        if len(route_waypoints) < 2:
            raise ValueError(f"Route {route['name']} must have at least 2 waypoints")
        
        # For now, just use start and end points (straight line)
        start_wp = route_waypoints[0]
        end_wp = route_waypoints[-1]
        
        start_lat, start_lon = start_wp['lat'], start_wp['lon']
        end_lat, end_lon = end_wp['lat'], end_wp['lon']
        
        # Calculate total distance and time needed
        total_distance = self._haversine_distance(start_lat, start_lon, end_lat, end_lon)
        total_time_needed_ms = (total_distance / self.vehicle_speed_ms) * 1000
        
        # Calculate movement per timestamp
        lat_diff = end_lat - start_lat
        lon_diff = end_lon - start_lon
        
        # Generate trajectory points
        for timestamp in self.timestamps:
            if total_time_needed_ms > 0:
                progress = min(timestamp / total_time_needed_ms, 1.0)
            else:
                progress = 1.0  # Instant movement for very short distances
            
            current_lat = start_lat + (lat_diff * progress)
            current_lon = start_lon + (lon_diff * progress)
            
            trajectory.append((timestamp, current_lat, current_lon))
            
            # Stop generating points once destination is reached
            if progress >= 1.0:
                # Fill remaining timestamps with end position
                while len(trajectory) < len(self.timestamps):
                    trajectory.append((self.timestamps[len(trajectory)], end_lat, end_lon))
                break
                
        return trajectory
    
    def generate_rsu_trajectory(self, rsu_id: int) -> List[Tuple[int, float, float]]:
        """Generate static trajectory for RSU (same position for all timestamps)."""
        trajectory = []
        rsu_config = self.config.rsu
        
        for timestamp in self.timestamps:
            trajectory.append((timestamp, rsu_config['position']['lat'], rsu_config['position']['lon']))
            
        return trajectory
    
    def save_trajectory_csv(self, trajectory: List[Tuple[int, float, float]], 
                           filename: str, entity_type: str, entity_id: int):
        """Save trajectory to CSV file."""
        os.makedirs(os.path.dirname(filename), exist_ok=True)
        
        with open(filename, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            # Write header
            writer.writerow(['timestamp_ms', 'latitude', 'longitude'])
            # Write trajectory data
            for timestamp, lat, lon in trajectory:
                writer.writerow([timestamp, f"{lat:.8f}", f"{lon:.8f}"])
        
        print(f"Generated {entity_type} {entity_id} trajectory: {filename} ({len(trajectory)} points)")
    
    @staticmethod
    def _haversine_distance(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
        """Calculate distance between two points using Haversine formula."""
        R = 6371000  # Earth radius in meters
        
        lat1_rad = math.radians(lat1)
        lat2_rad = math.radians(lat2)
        dlat = math.radians(lat2 - lat1)
        dlon = math.radians(lon2 - lon1)
        
        a = (math.sin(dlat/2) * math.sin(dlat/2) + 
             math.cos(lat1_rad) * math.cos(lat2_rad) * 
             math.sin(dlon/2) * math.sin(dlon/2))
        c = 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))
        
        return R * c

def main():
    parser = argparse.ArgumentParser(description='Generate simple trajectory files for Map 1')
    parser.add_argument('--config', type=str, default='config/map_1_config.json',
                       help='Configuration file path (default: config/map_1_config.json)')
    parser.add_argument('--vehicles', type=int, 
                       help='Number of vehicles (overrides config default)')
    parser.add_argument('--duration', type=int, 
                       help='Simulation duration in seconds (overrides config default)')
    parser.add_argument('--output-dir', type=str, 
                       help='Output directory for trajectory files (overrides config default)')
    parser.add_argument('--update-interval', type=int,
                       help='Update interval in milliseconds (overrides config default)')
    
    args = parser.parse_args()
    
    # Load configuration
    if not os.path.exists(args.config):
        print(f"Error: Configuration file not found: {args.config}")
        print("Make sure the config file exists. You can use config/map_1_config.json")
        return 1
    
    config = SimpleMapConfig(args.config)
    
    # Override config values with command line arguments if provided
    n_vehicles = args.vehicles if args.vehicles is not None else config.vehicles['default_count']
    duration = args.duration if args.duration is not None else config.simulation['duration_s']
    output_dir = args.output_dir if args.output_dir is not None else config.logging['trajectory_dir']
    update_interval = args.update_interval if args.update_interval is not None else config.simulation['update_interval_ms']
    
    print(f"Generating simple trajectories for Map 1:")
    print(f"  Config file: {args.config}")
    print(f"  Vehicles: {n_vehicles}" + (" (from config)" if args.vehicles is None else " (from command line)"))
    print(f"  Duration: {duration} seconds" + (" (from config)" if args.duration is None else " (from command line)"))
    print(f"  Update interval: {update_interval} ms" + (" (from config)" if args.update_interval is None else " (from command line)"))
    print(f"  Output directory: {output_dir}" + (" (from config)" if args.output_dir is None else " (from command line)"))
    print(f"  Vehicle speed: {config.vehicles['speed_kmh']} km/h")
    print(f"  Available routes: {[route['name'] for route in config.routes]}")
    
    generator = SimpleTrajectoryGenerator(config, duration, update_interval)
    
    # Generate RSU trajectory (static)
    rsu_id = config.rsu['id']
    rsu_trajectory = generator.generate_rsu_trajectory(rsu_id)
    rsu_filename = os.path.join(output_dir, f"rsu_{rsu_id}_trajectory.csv")
    generator.save_trajectory_csv(rsu_trajectory, rsu_filename, "RSU", rsu_id)
    
    # Generate vehicle trajectories
    for vehicle_id in range(1, n_vehicles + 1):
        vehicle_trajectory = generator.generate_vehicle_trajectory(vehicle_id)
        vehicle_filename = os.path.join(output_dir, f"vehicle_{vehicle_id}_trajectory.csv")
        generator.save_trajectory_csv(vehicle_trajectory, vehicle_filename, "Vehicle", vehicle_id)
    
    print(f"\nTrajectory generation completed!")
    print(f"Files saved to: {output_dir}")
    print(f"Total files: {n_vehicles + 1} (vehicles + RSU)")
    
    # Print summary statistics
    print(f"\nMap 1 Summary:")
    print(f"  Description: {config.config['map_info']['description']}")
    print(f"  RSU Position: ({config.rsu['position']['lat']:.6f}, {config.rsu['position']['lon']:.6f})")
    print(f"  RSU ID: {config.rsu['id']}")
    print(f"  Vehicle Speed: {config.vehicles['speed_kmh']} km/h")
    print(f"  Transmission Radius: {config.simulation['default_transmission_radius_m']}m (all entities)")
    print(f"  Routes: {len(config.routes)} available")
    print(f"  Waypoints: {len(config.waypoints)} defined")
    
    return 0

if __name__ == "__main__":
    exit(main())