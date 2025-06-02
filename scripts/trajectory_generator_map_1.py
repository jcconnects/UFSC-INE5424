#!/usr/bin/env python3
"""
Trajectory Generator for Map 1 - Urban Grid Scenario
Generates CSV trajectory files for vehicles and RSUs for radius-based collision domain simulation.

Usage:
    python3 trajectory_generator_map_1.py --vehicles <num> --duration <seconds> --output-dir <path>
"""

import argparse
import csv
import math
import random
import os
from typing import Tuple, List

class TrajectoryGenerator:
    """Generates realistic vehicle trajectories and static RSU positions."""
    
    def __init__(self, duration_seconds: int, update_interval_ms: int = 100):
        self.duration_ms = duration_seconds * 1000
        self.update_interval_ms = update_interval_ms
        self.timestamps = list(range(0, self.duration_ms + 1, update_interval_ms))
        
        # Map 1: Simple urban grid (1km x 1km area)
        self.map_bounds = {
            'lat_min': -27.5969,  # Florianópolis approximate area
            'lat_max': -27.5869,  # ~1.1km north-south
            'lon_min': -48.5482,  # ~1.1km east-west  
            'lon_max': -48.5382
        }
        
        # RSU positioned at intersection center for maximum coverage
        self.rsu_position = {
            'lat': (self.map_bounds['lat_min'] + self.map_bounds['lat_max']) / 2,
            'lon': (self.map_bounds['lon_min'] + self.map_bounds['lon_max']) / 2
        }
        
    def generate_vehicle_trajectory(self, vehicle_id: int) -> List[Tuple[int, float, float]]:
        """Generate a realistic trajectory for a vehicle."""
        trajectory = []
        
        # Random starting position at map edge
        start_edge = random.choice(['north', 'south', 'east', 'west'])
        if start_edge == 'north':
            start_lat = self.map_bounds['lat_max']
            start_lon = random.uniform(self.map_bounds['lon_min'], self.map_bounds['lon_max'])
        elif start_edge == 'south':
            start_lat = self.map_bounds['lat_min']
            start_lon = random.uniform(self.map_bounds['lon_min'], self.map_bounds['lon_max'])
        elif start_edge == 'east':
            start_lat = random.uniform(self.map_bounds['lat_min'], self.map_bounds['lat_max'])
            start_lon = self.map_bounds['lon_max']
        else:  # west
            start_lat = random.uniform(self.map_bounds['lat_min'], self.map_bounds['lat_max'])
            start_lon = self.map_bounds['lon_min']
        
        # Random destination (usually across the map)
        dest_lat = random.uniform(self.map_bounds['lat_min'], self.map_bounds['lat_max'])
        dest_lon = random.uniform(self.map_bounds['lon_min'], self.map_bounds['lon_max'])
        
        # Vehicle speed: 30-60 km/h (urban traffic)
        speed_kmh = random.uniform(30, 60)
        speed_ms = speed_kmh / 3.6  # Convert to m/s
        
        # Calculate movement per time step
        lat_diff = dest_lat - start_lat
        lon_diff = dest_lon - start_lon
        total_distance = self._haversine_distance(start_lat, start_lon, dest_lat, dest_lon)
        total_time_needed = total_distance / speed_ms * 1000  # ms
        
        if total_time_needed > self.duration_ms:
            # Scale down movement if trajectory is too long
            scale = self.duration_ms / total_time_needed
            lat_diff *= scale
            lon_diff *= scale
            
        # Generate trajectory points
        for timestamp in self.timestamps:
            progress = min(timestamp / self.duration_ms, 1.0)
            
            # Add some random variation for realistic movement
            noise_factor = 0.0001  # Small GPS noise
            lat_noise = random.uniform(-noise_factor, noise_factor)
            lon_noise = random.uniform(-noise_factor, noise_factor)
            
            current_lat = start_lat + (lat_diff * progress) + lat_noise
            current_lon = start_lon + (lon_diff * progress) + lon_noise
            
            # Ensure coordinates stay within map bounds
            current_lat = max(self.map_bounds['lat_min'], 
                            min(self.map_bounds['lat_max'], current_lat))
            current_lon = max(self.map_bounds['lon_min'], 
                            min(self.map_bounds['lon_max'], current_lon))
            
            trajectory.append((timestamp, current_lat, current_lon))
            
        return trajectory
    
    def generate_rsu_trajectory(self, rsu_id: int) -> List[Tuple[int, float, float]]:
        """Generate static trajectory for RSU (same position for all timestamps)."""
        trajectory = []
        
        for timestamp in self.timestamps:
            trajectory.append((timestamp, self.rsu_position['lat'], self.rsu_position['lon']))
            
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
    parser = argparse.ArgumentParser(description='Generate trajectory files for Map 1')
    parser.add_argument('--vehicles', type=int, default=30, 
                       help='Number of vehicles (default: 30)')
    parser.add_argument('--duration', type=int, default=30, 
                       help='Simulation duration in seconds (default: 30)')
    parser.add_argument('--output-dir', type=str, default='tests/logs/trajectories', 
                       help='Output directory for trajectory files')
    parser.add_argument('--update-interval', type=int, default=100,
                       help='Update interval in milliseconds (default: 100)')
    
    args = parser.parse_args()
    
    print(f"Generating trajectories for Map 1:")
    print(f"  Vehicles: {args.vehicles}")
    print(f"  Duration: {args.duration} seconds")
    print(f"  Update interval: {args.update_interval} ms")
    print(f"  Output directory: {args.output_dir}")
    
    generator = TrajectoryGenerator(args.duration, args.update_interval)
    
    # Generate RSU trajectory (static)
    rsu_id = 1000  # Match the RSU ID used in demo.cpp
    rsu_trajectory = generator.generate_rsu_trajectory(rsu_id)
    rsu_filename = os.path.join(args.output_dir, f"rsu_{rsu_id}_trajectory.csv")
    generator.save_trajectory_csv(rsu_trajectory, rsu_filename, "RSU", rsu_id)
    
    # Generate vehicle trajectories
    for vehicle_id in range(1, args.vehicles + 1):
        vehicle_trajectory = generator.generate_vehicle_trajectory(vehicle_id)
        vehicle_filename = os.path.join(args.output_dir, f"vehicle_{vehicle_id}_trajectory.csv")
        generator.save_trajectory_csv(vehicle_trajectory, vehicle_filename, "Vehicle", vehicle_id)
    
    print(f"\nTrajectory generation completed!")
    print(f"Files saved to: {args.output_dir}")
    print(f"Total files: {args.vehicles + 1} (vehicles + RSU)")
    
    # Print summary statistics
    print(f"\nMap 1 Summary:")
    print(f"  Area: Florianópolis region (~1.1km x 1.1km)")
    print(f"  RSU Position: ({generator.rsu_position['lat']:.6f}, {generator.rsu_position['lon']:.6f})")
    print(f"  Vehicle Speed Range: 30-60 km/h")
    print(f"  Trajectory Points per Entity: {len(generator.timestamps)}")

if __name__ == "__main__":
    main() 