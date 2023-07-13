#### 一，远程仓库相关操作
##### 1.克隆远程仓库代码到本机
git clone url

##### 2.显示远程仓库
git remote -v

##### 3.从远程仓库拉取数据
git pull origin master
git pull origin master --allow-unrelated-histories

##### 4.推送数据到远程仓库
git pull origin master //先拉取
git push origin master //后推送

### 二，分支管理
##### 1.查看分支
git branch

##### 2.创建分支
git branch dev

##### 3.切换分支
git checkout dev

##### 4.分支合并
git merge dev

### 三.关联远程分支
##### 1.克隆的方式：如首次clone项目的master分支就已经本地的和远程的分支是关联一起的，因为是用拉取下来的。
git clone xxx.git

##### 2.本地推送的方式：我们在本地创建一个分支，然后推送到远程仓库，然后再进行关联。
git checkout -b dev #创建并切换到dev分支 
git push origin dev #推送到远程仓库
git branch -u origin/dev #dev关联到远程dev分支

###### 3.远程拉取方式：远程仓库已经存在一个分支，通过命令拉取到本地。这种情况出现在多人开发中，你的同事给远程推送了一个分支。然后你这边拉取下来，然后再进行关联。
git fetch origin  #把远程仓库的数据在本地进行更新
git checkout -b dev origin/dev  #创建dev分支并且将远程origin/dev分支关联到本地dev分支
[ git merge origin/dev ] #将远程origin/dev分支代码合并到本地dev分支 

### 四.删除分支
1.git branch -d bug #删除本地dev分支
2.git push origin --delete bug #删除远程dev分支
