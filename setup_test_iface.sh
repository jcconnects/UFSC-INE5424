#!/bin/bash

# This script handles the setup and cleanup of test network interfaces
# It's called by the Makefile to avoid complex shell commands in the Makefile itself

LOGS_DIR="tests/logs"
mkdir -p "$LOGS_DIR"
sudo chmod 777 "$LOGS_DIR"  # Ensure logs directory is writable

# Default interface name
DEFAULT_IFACE="test-dummy0"

function setup_interface() {
    IFACE_TO_USE="$DEFAULT_IFACE"
    
    echo "Attempting to set up interface. Initial target: $IFACE_TO_USE"
    
    # Create a temporary log file for this run
    TEMP_LOG=$(mktemp)
    trap 'rm -f "$TEMP_LOG"' EXIT
    
    # Check if interface exists and is a dummy
    if ip link show "$IFACE_TO_USE" > "$TEMP_LOG" 2>&1; then
        echo "Interface $IFACE_TO_USE already exists, checking type..."
        if ip link show "$IFACE_TO_USE" | grep -q "dummy"; then
            echo "Existing $IFACE_TO_USE is a dummy interface, reusing it."
            sudo ip link set "$IFACE_TO_USE" up > "$TEMP_LOG" 2>&1 || {
                echo "Failed to bring up interface $IFACE_TO_USE"
                cat "$TEMP_LOG"
                return 1
            }
        else
            echo "WARNING: $IFACE_TO_USE exists but is NOT a dummy interface. Trying alternate."
            IFACE_TO_USE="test-dummy1"
            
            if ip link show "$IFACE_TO_USE" > "$TEMP_LOG" 2>&1; then
                echo "Alternate interface $IFACE_TO_USE also exists, checking type..."
                if ip link show "$IFACE_TO_USE" | grep -q "dummy"; then
                    echo "Existing $IFACE_TO_USE is a dummy interface, reusing it."
                    sudo ip link set "$IFACE_TO_USE" up > "$TEMP_LOG" 2>&1 || {
                        echo "Failed to bring up interface $IFACE_TO_USE"
                        cat "$TEMP_LOG"
                        return 1
                    }
                else
                    echo "WARNING: $IFACE_TO_USE is also NOT a dummy interface."
                    echo "Manual cleanup of interfaces might be needed."
                    cat "$TEMP_LOG"
                    return 1
                fi
            else
                echo "Creating new dummy interface $IFACE_TO_USE..."
                sudo ip link add "$IFACE_TO_USE" type dummy > "$TEMP_LOG" 2>&1 || {
                    echo "Failed to create interface $IFACE_TO_USE"
                    cat "$TEMP_LOG"
                    return 1
                }
                sudo ip link set "$IFACE_TO_USE" up > "$TEMP_LOG" 2>&1 || {
                    echo "Failed to bring up interface $IFACE_TO_USE"
                    cat "$TEMP_LOG"
                    return 1
                }
            fi
        fi
    else
        echo "Creating new dummy interface $IFACE_TO_USE..."
        sudo ip link add "$IFACE_TO_USE" type dummy > "$TEMP_LOG" 2>&1 || {
            echo "Failed to create interface $IFACE_TO_USE"
            cat "$TEMP_LOG"
            return 1
        }
        sudo ip link set "$IFACE_TO_USE" up > "$TEMP_LOG" 2>&1 || {
            echo "Failed to bring up interface $IFACE_TO_USE"
            cat "$TEMP_LOG"
            return 1
        }
    fi
    
    echo "Final interface to use: $IFACE_TO_USE"
    echo "$IFACE_TO_USE" > "$LOGS_DIR/current_test_iface"
    echo "Interface $IFACE_TO_USE is configured and ready"
    return 0
}

function cleanup_interface() {
    IFACE_FILE="$LOGS_DIR/current_test_iface"
    
    if [ -f "$IFACE_FILE" ]; then
        ACTUAL_TEST_IFACE=$(cat "$IFACE_FILE")
        echo "Found interface '$ACTUAL_TEST_IFACE' in $IFACE_FILE."
        
        if ip link show "$ACTUAL_TEST_IFACE" > /dev/null 2>&1; then
            if ip link show "$ACTUAL_TEST_IFACE" | grep -q "dummy"; then
                echo "Removing dummy interface $ACTUAL_TEST_IFACE..."
                sudo ip link delete "$ACTUAL_TEST_IFACE"
            else
                echo "WARNING: Interface $ACTUAL_TEST_IFACE is not a dummy interface. Not removing."
            fi
        else
            echo "Interface $ACTUAL_TEST_IFACE does not exist. No need to remove."
        fi
        
        rm -f "$IFACE_FILE"
    else
        echo "Warning: $IFACE_FILE not found. Cannot determine specific interface to clean."
        echo "Attempting to clean known default dummy interface names"
        
        for iface_name_to_check in "$DEFAULT_IFACE" "test-dummy1"; do
            if ip link show "$iface_name_to_check" > /dev/null 2>&1; then
                if ip link show "$iface_name_to_check" | grep -q "dummy"; then
                    echo "Cleaning up known dummy interface: $iface_name_to_check"
                    sudo ip link delete "$iface_name_to_check"
                fi
            fi
        done
    fi
}

# Main script logic
if [ "$1" = "setup" ]; then
    setup_interface
elif [ "$1" = "cleanup" ]; then
    cleanup_interface
else
    echo "Usage: $0 [setup|cleanup]"
    exit 1
fi

exit 0 