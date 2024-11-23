CREATE TABLE IF NOT EXISTS requests_uniq (
    r   TEXT
);

CREATE TRIGGER IF NOT EXISTS insert_request
AFTER INSERT ON combined
WHEN (NEW.request NOT IN (SELECT * FROM requests_uniq))
BEGIN
    INSERT INTO requests_uniq VALUES (NEW.request);
END;

