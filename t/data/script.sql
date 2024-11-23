CREATE VIEW IF NOT EXISTS v_notfound AS
    SELECT remote_addr, time_local, request
    FROM combined
    WHERE status=404

