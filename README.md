To run this, use the following commands.

# builds the image
docker build . -t mpl

# run the image in the background
docker run -d -it --name=mpl mpl:latest 

# ssh into the container
docker exec -it mpl /bin/bash

Do whatever you want!

# When done, kill the container
docker kill mpl


Use the --cpus="N" argument when running the instance to set the number
of CPU's allowed to the instance.
