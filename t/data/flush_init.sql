CREATE TABLE IF NOT EXISTS times (
    t   TEXT
);

INSERT INTO times VALUES ((
    SELECT time()
));

