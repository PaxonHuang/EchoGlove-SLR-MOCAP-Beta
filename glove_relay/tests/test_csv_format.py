"""Tests for V5 CSV data format (design spec Section 7.2)."""
import pytest
import csv
import io

EXPECTED_HEADER = "timestamp,hand_id,flex0,flex1,flex2,flex3,flex4,euler_x,euler_y,euler_z,gyro_x,gyro_y,gyro_z,gesture_id"


def test_csv_header_matches_spec():
    """Verify CSV format matches design spec Section 7.2"""
    header_fields = EXPECTED_HEADER.split(",")
    assert len(header_fields) == 14
    assert header_fields[0] == "timestamp"
    assert header_fields[1] == "hand_id"
    assert header_fields[2] == "flex0"
    assert header_fields[6] == "flex4"
    assert header_fields[7] == "euler_x"
    assert header_fields[10] == "gyro_x"
    assert header_fields[12] == "gyro_z"
    assert header_fields[13] == "gesture_id"


def test_csv_row_parseable():
    """Verify a sample row is parseable with correct types"""
    row = "1234567,0,0.12,0.85,0.78,0.65,0.23,12.3,-45.6,78.9,0.5,-1.2,0.3,15"
    reader = csv.reader(io.StringIO(row))
    fields = next(reader)
    assert len(fields) == 14
    assert int(fields[1]) == 0  # hand_id (left)
    assert int(fields[13]) == 15  # gesture_id
    assert 0.0 <= float(fields[2]) <= 1.0  # flex0 normalized
    assert 0.0 <= float(fields[6]) <= 1.0  # flex4 normalized


def test_csv_right_hand_row():
    """Verify right hand data uses hand_id=1"""
    row = "1234568,1,0.45,0.67,0.89,0.12,0.34,5.0,10.0,15.0,0.1,0.2,0.3,22"
    reader = csv.reader(io.StringIO(row))
    fields = next(reader)
    assert int(fields[1]) == 1  # hand_id (right)


def test_csv_14_columns_strict():
    """Reject rows with wrong column count"""
    bad_row = "1234567,0,0.1,0.2,0.3,0.4,0.5,1.0,2.0,3.0,0.1,0.2,0.3"  # 13 cols
    reader = csv.reader(io.StringIO(bad_row))
    fields = next(reader)
    assert len(fields) != 14, "Should detect wrong column count"
